// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib/heap.h>

#include <lauf/runtime/builtin.h>
#include <lauf/runtime/process.hpp>
#include <lauf/vm.hpp>

LAUF_RUNTIME_BUILTIN(lauf_lib_heap_alloc, 2, 1, LAUF_RUNTIME_BUILTIN_VM_ONLY, "alloc", nullptr)
{
    auto size      = vstack_ptr[0].as_uint;
    auto alignment = vstack_ptr[1].as_uint;

    auto memory
        = process->vm->allocator.heap_alloc(process->vm->allocator.user_data, size, alignment);
    if (memory == nullptr)
        return lauf_runtime_panic(process, "out of memory");

    auto alloc   = lauf::make_heap_alloc(memory, size, process->alloc_generation);
    auto address = process->add_allocation(alloc);

    ++vstack_ptr;
    vstack_ptr[0].as_address = address;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_heap_alloc_array, 3, 1, LAUF_RUNTIME_BUILTIN_VM_ONLY, "alloc_array",
                     &lauf_lib_heap_alloc)
{
    auto count     = vstack_ptr[0].as_uint;
    auto size      = vstack_ptr[1].as_uint;
    auto alignment = vstack_ptr[2].as_uint;

    auto memory_size = lauf::round_to_multiple_of_alignment(size, alignment) * count;

    ++vstack_ptr;
    vstack_ptr[0].as_uint = memory_size;

    LAUF_TAIL_CALL return lauf_lib_heap_alloc.impl(ip, vstack_ptr, frame_ptr, process);
}

LAUF_RUNTIME_BUILTIN(lauf_lib_heap_free, 1, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY, "free",
                     &lauf_lib_heap_alloc_array)
{
    auto address = vstack_ptr[0].as_address;
    ++vstack_ptr;

    auto alloc = process->get_allocation(address);
    if (alloc == nullptr || alloc->status != lauf::allocation_status::allocated
        || alloc->source != lauf::allocation_source::heap_memory)
        return lauf_runtime_panic(process, "invalid heap address");

    process->vm->allocator.free_alloc(process->vm->allocator.user_data, alloc->ptr, alloc->size);
    alloc->status = lauf::allocation_status::freed;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_heap_leak, 1, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY, "leak",
                     &lauf_lib_heap_free)
{
    auto address = vstack_ptr[0].as_address;
    ++vstack_ptr;

    auto alloc = process->get_allocation(address);
    if (alloc == nullptr || alloc->status != lauf::allocation_status::allocated
        || alloc->source != lauf::allocation_source::heap_memory)
        return lauf_runtime_panic(process, "invalid heap address");

    alloc->status = lauf::allocation_status::freed;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_heap_transfer_local, 1, 1, LAUF_RUNTIME_BUILTIN_VM_ONLY,
                     "transfer_local", &lauf_lib_heap_leak)
{
    auto address = vstack_ptr[0].as_address;

    auto alloc = process->get_allocation(address);
    if (alloc == nullptr || alloc->status != lauf::allocation_status::allocated)
        return lauf_runtime_panic(process, "invalid address");

    if (alloc->source == lauf::allocation_source::local_memory)
    {
        auto memory = process->vm->allocator.heap_alloc(process->vm->allocator.user_data,
                                                        alloc->size, alignof(void*));
        if (memory == nullptr)
            return lauf_runtime_panic(process, "out of memory");

        std::memcpy(memory, alloc->ptr, alloc->size);

        auto heap_alloc = lauf::make_heap_alloc(memory, alloc->size, process->alloc_generation);
        vstack_ptr[0].as_address = process->add_allocation(heap_alloc);
    }

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_heap_poison, 1, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY, "poison",
                     &lauf_lib_heap_transfer_local)
{
    auto address = vstack_ptr[0].as_address;
    ++vstack_ptr;

    auto alloc = process->get_allocation(address);
    if (alloc == nullptr || alloc->source != lauf::allocation_source::heap_memory
        || alloc->status != lauf::allocation_status::allocated)
        return lauf_runtime_panic(process, "invalid heap address");
    alloc->status = lauf::allocation_status::poison;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_heap_unpoison, 1, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY, "unpoison",
                     &lauf_lib_heap_poison)
{
    auto address = vstack_ptr[0].as_address;
    ++vstack_ptr;

    auto alloc = process->get_allocation(address);
    if (alloc == nullptr || alloc->status != lauf::allocation_status::poison)
        // (We don't need to check for heap memory, only heap memory may be poisoned anyway.)
        return lauf_runtime_panic(process, "invalid heap address");
    alloc->status = lauf::allocation_status::allocated;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

const lauf_runtime_builtin_library lauf_lib_heap = {"lauf.heap", &lauf_lib_heap_unpoison};

