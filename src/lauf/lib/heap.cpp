// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib/heap.h>

#include <lauf/runtime/builtin.h>
#include <lauf/runtime/process.hpp>
#include <lauf/support/array_list.hpp>
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
    if (alloc == nullptr || !lauf::can_be_freed(alloc->status)
        || alloc->source != lauf::allocation_source::heap_memory
        || alloc->split != lauf::allocation_split::unsplit)
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
    if (alloc == nullptr || !lauf::can_be_freed(alloc->status)
        || alloc->source != lauf::allocation_source::heap_memory
        || alloc->split != lauf::allocation_split::unsplit)
        return lauf_runtime_panic(process, "invalid heap address");

    alloc->status = lauf::allocation_status::freed;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_heap_transfer_local, 1, 1, LAUF_RUNTIME_BUILTIN_VM_ONLY,
                     "transfer_local", &lauf_lib_heap_leak)
{
    auto address = vstack_ptr[0].as_address;

    auto alloc = process->get_allocation(address);
    if (alloc == nullptr || !lauf::is_usable(alloc->status))
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

LAUF_RUNTIME_BUILTIN(lauf_lib_heap_gc, 0, 1, LAUF_RUNTIME_BUILTIN_VM_ONLY, "gc",
                     &lauf_lib_heap_transfer_local)
{
    auto                                marker = process->vm->get_marker();
    lauf::array_list<lauf::allocation*> pending_allocations;

    auto mark_reachable = [&](lauf::allocation* alloc) {
        if (alloc->status != lauf::allocation_status::freed
            && alloc->gc == lauf::gc_tracking::unreachable)
        {
            // It is a not freed allocation that we didn't know was reachable yet, add it.
            alloc->gc = lauf::gc_tracking::reachable;
            pending_allocations.push_back(*process->vm, alloc);
        }
    };
    auto process_reachable_memory = [&](lauf::allocation* alloc) {
        if (alloc->size < sizeof(lauf_runtime_address) || alloc->is_gc_weak)
            return;

        // Assume the allocation contains an array of values.
        // For that we need to align the pointer properly.
        // (We've done a size check already, so the initial offset is fine)
        auto offset = lauf::align_offset(alloc->ptr, alignof(lauf_runtime_value));
        auto ptr    = reinterpret_cast<lauf_runtime_value*>(static_cast<unsigned char*>(alloc->ptr)
                                                         + offset);

        for (auto end = ptr + (alloc->size - offset) / sizeof(lauf_runtime_value); ptr != end;
             ++ptr)
        {
            auto alloc = process->get_allocation(ptr->as_address);
            if (alloc != nullptr && ptr->as_address.offset <= alloc->size)
                mark_reachable(alloc);
        }
    };

    // Mark allocations reachable by addresses in the vstack as reachable.
    for (auto cur = vstack_ptr; cur != lauf_runtime_get_vstack_base(process); ++cur)
    {
        auto alloc = process->get_allocation(cur->as_address);
        if (alloc != nullptr && cur->as_address.offset <= alloc->size)
            mark_reachable(alloc);
    }

    // Process memory from explicitly reachable allocations.
    for (auto& alloc : process->allocations)
    {
        // Non-heap memory should always be reachable.
        assert(alloc.source == lauf::allocation_source::heap_memory
               || alloc.gc == lauf::gc_tracking::reachable_explicit);

        if (alloc.gc == lauf::gc_tracking::reachable_explicit
            && alloc.status != lauf::allocation_status::freed)
            process_reachable_memory(&alloc);
    }

    // Recursively mark everything as reachable from reachable allocations.
    while (!pending_allocations.empty())
    {
        auto alloc = pending_allocations.back();
        pending_allocations.pop_back();
        process_reachable_memory(alloc);
    }

    // Free unreachable heap memory.
    auto allocator   = process->vm->allocator;
    auto bytes_freed = std::size_t(0);
    for (auto& alloc : process->allocations)
    {
        if (alloc.source == lauf::allocation_source::heap_memory
            && alloc.status != lauf::allocation_status::freed
            && alloc.split == lauf::allocation_split::unsplit
            && alloc.gc == lauf::gc_tracking::unreachable)
        {
            // It is an unreachable allocation that we can free.
            allocator.free_alloc(allocator.user_data, alloc.ptr, alloc.size);
            alloc.status = lauf::allocation_status::freed;
            bytes_freed += alloc.size;
        }

        // Need to reset GC tracking for next GC run.
        if (alloc.gc != lauf::gc_tracking::reachable_explicit)
            alloc.gc = lauf::gc_tracking::unreachable;
    }

    // Set result.
    --vstack_ptr;
    vstack_ptr[0].as_uint = bytes_freed;

    process->vm->unwind(marker);
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_heap_declare_reachable, 1, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY,
                     "declare_reachable", &lauf_lib_heap_gc)
{
    auto ptr = vstack_ptr[0].as_address;
    ++vstack_ptr;

    auto alloc = process->get_allocation(ptr);
    if (alloc == nullptr || alloc->source != lauf::allocation_source::heap_memory)
        return lauf_runtime_panic(process, "invalid heap address");
    alloc->gc = lauf::gc_tracking::reachable_explicit;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_heap_undeclare_reachable, 1, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY,
                     "undeclare_reachable", &lauf_lib_heap_declare_reachable)
{
    auto ptr = vstack_ptr[0].as_address;
    ++vstack_ptr;

    auto alloc = process->get_allocation(ptr);
    if (alloc == nullptr || alloc->source != lauf::allocation_source::heap_memory)
        return lauf_runtime_panic(process, "invalid heap address");
    alloc->gc = lauf::gc_tracking::unreachable;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_heap_declare_weak, 1, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY, "declare_weak",
                     &lauf_lib_heap_undeclare_reachable)
{
    auto ptr = vstack_ptr[0].as_address;
    ++vstack_ptr;

    auto alloc = process->get_allocation(ptr);
    if (alloc == nullptr)
        return lauf_runtime_panic(process, "invalid address");
    alloc->is_gc_weak = true;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_heap_undeclare_weak, 1, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY,
                     "undeclare_weak", &lauf_lib_heap_declare_weak)
{
    auto ptr = vstack_ptr[0].as_address;
    ++vstack_ptr;

    auto alloc = process->get_allocation(ptr);
    if (alloc == nullptr)
        return lauf_runtime_panic(process, "invalid address");
    alloc->is_gc_weak = false;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

const lauf_runtime_builtin_library lauf_lib_heap = {"lauf.heap", &lauf_lib_heap_undeclare_weak};

