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

LAUF_RUNTIME_BUILTIN(lauf_lib_heap_free, 1, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY, "free",
                     &lauf_lib_heap_alloc)
{
    auto address = vstack_ptr[0].as_address;
    ++vstack_ptr;

    auto alloc = process->get_allocation(address.allocation);
    if (alloc == nullptr || alloc->generation != address.generation
        || alloc->status != lauf::allocation_status::allocated
        || alloc->source != lauf::allocation_source::heap_memory)
        return lauf_runtime_panic(process, "invalid heap address");

    process->vm->allocator.free_alloc(process->vm->allocator.user_data, alloc->ptr, alloc->size);
    alloc->status = lauf::allocation_status::freed;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

const lauf_runtime_builtin_library lauf_lib_heap = {"lauf.heap", &lauf_lib_heap_free};
