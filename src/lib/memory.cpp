// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib/memory.h>

#include <lauf/bc/vm_memory.hpp>
#include <lauf/impl/builtin.hpp>
#include <lauf/impl/process.hpp>
#include <lauf/impl/vm.hpp>

LAUF_BUILTIN_UNARY_OPERATION(lauf_address_to_int_builtin, 2, {
    result[1].as_uint = lauf_value_uint(value.as_address.allocation) << 34
                        | lauf_value_uint(value.as_address.generation) << 32;
    result[0].as_uint = value.as_address.offset;
})

LAUF_BUILTIN_BINARY_OPERATION(lauf_address_from_int_builtin, 1, {
    lauf_value_address addr;
    addr.allocation = lhs.as_uint >> 34;
    addr.generation = lhs.as_uint & 0b11;
    addr.offset     = rhs.as_uint;
    if (addr.offset != rhs.as_uint)
        LAUF_BUILTIN_OPERATION_PANIC("adress offset overflow");
    result->as_address = addr;
})

LAUF_BUILTIN_BINARY_OPERATION(lauf_heap_alloc_builtin, 1, {
    auto vm = process->vm();

    auto size      = lhs.as_uint;
    auto alignment = rhs.as_uint;
    auto ptr       = vm->allocator.heap_alloc(vm->allocator.user_data, size, alignment);
    if (ptr == nullptr)
        LAUF_BUILTIN_OPERATION_PANIC("out of heap memory");

    auto alloc = lauf::vm_allocation(ptr, uint32_t(size), lauf::vm_allocation::heap_memory);
    if (!process->has_capacity_for_allocations(1))
        lauf_vm_process_impl::resize_allocation_list(process);
    result->as_address = process->add_allocation(alloc);
})

LAUF_BUILTIN_UNARY_OPERATION(lauf_free_alloc_builtin, 0, {
    auto vm   = process->vm();
    auto addr = value.as_address;

    auto alloc = process->get_allocation(addr);
    if (alloc == nullptr
        || alloc->source != lauf::vm_allocation::heap_memory
        // We do not allow freeing split memory as others might be using other parts.
        || alloc->split != lauf::vm_allocation::unsplit)
        LAUF_BUILTIN_OPERATION_PANIC("invalid address");

    vm->allocator.free_alloc(vm->allocator.user_data, alloc->ptr);
    process->remove_allocation(addr);
})

LAUF_BUILTIN_UNARY_OPERATION(lauf_leak_alloc_builtin, 0, {
    auto vm   = process->vm();
    auto addr = value.as_address;

    auto alloc = process->get_allocation(addr);
    if (alloc == nullptr
        || alloc->source != lauf::vm_allocation::heap_memory
        // We do not allow leaking split memory as others might be using other parts.
        || alloc->split != lauf::vm_allocation::unsplit)
        LAUF_BUILTIN_OPERATION_PANIC("invalid address");

    alloc->lifetime = lauf::vm_allocation::leaked;
})

LAUF_BUILTIN_BINARY_OPERATION(lauf_split_alloc_builtin, 2, {
    auto length    = lhs.as_uint;
    auto base_addr = rhs.as_address;

    auto base_alloc = process->get_allocation(base_addr);
    if (!base_alloc || length > base_alloc->size)
        LAUF_BUILTIN_OPERATION_PANIC("invalid address");

    auto alloc1 = *base_alloc;
    alloc1.size = length;

    auto alloc2 = *base_alloc;
    alloc2.ptr  = base_alloc->offset(length);
    alloc2.size -= length;

    switch (base_alloc->split)
    {
    case lauf::vm_allocation::unsplit:
        alloc1.split = lauf::vm_allocation::first_split;
        alloc2.split = lauf::vm_allocation::last_split;
        break;
    case lauf::vm_allocation::first_split:
        alloc1.split = lauf::vm_allocation::first_split;
        alloc2.split = lauf::vm_allocation::middle_split;
        break;
    case lauf::vm_allocation::middle_split:
        alloc1.split = lauf::vm_allocation::middle_split;
        alloc2.split = lauf::vm_allocation::middle_split;
        break;
    case lauf::vm_allocation::last_split:
        alloc1.split = lauf::vm_allocation::middle_split;
        alloc2.split = lauf::vm_allocation::last_split;
        break;
    }

    *base_alloc  = alloc1;
    auto addr1   = base_addr;
    addr1.offset = 0;

    if (!process->has_capacity_for_allocations(1))
        lauf_vm_process_impl::resize_allocation_list(process);
    auto addr2 = process->add_allocation(alloc2);

    result[1].as_address = addr1;
    result[0].as_address = addr2;
})

LAUF_BUILTIN_BINARY_OPERATION(lauf_merge_alloc_builtin, 1, {
    auto addr1 = lhs.as_address;
    auto addr2 = rhs.as_address;

    auto alloc1 = process->get_allocation(addr1);
    auto alloc2 = process->get_allocation(addr2);
    if (!alloc1
        || !alloc2
        // Both allocations need to be splits.
        || !alloc1->is_split()
        || !alloc2->is_split()
        // And they need to be next to each other.
        || alloc1->offset(alloc1->size) != alloc2->ptr)
        LAUF_BUILTIN_OPERATION_PANIC("invalid address");

    auto alloc1_first = alloc1->split == lauf::vm_allocation::first_split;
    auto alloc2_last  = alloc2->split == lauf::vm_allocation::last_split;

    auto& base_alloc = *alloc1;
    base_alloc.size += alloc2->size;
    if (alloc1_first && alloc2_last)
    {
        // If we're merging the first and last split, it's no longer split.
        base_alloc.split = lauf::vm_allocation::unsplit;
    }
    else if (!alloc1_first && alloc2_last)
    {
        // base_alloc is now the last split.
        base_alloc.split = lauf::vm_allocation::last_split;
    }
    auto base_addr = addr1;

    process->remove_allocation(addr2);

    result->as_address = base_addr;
})

LAUF_BUILTIN_UNARY_OPERATION(lauf_poison_alloc_builtin, 0, {
    auto addr = value.as_address;
    if (auto alloc = process->get_allocation(addr))
        alloc->lifetime = lauf::vm_allocation::poisoned;
    else
        LAUF_BUILTIN_OPERATION_PANIC("invalid address");
})

LAUF_BUILTIN_UNARY_OPERATION(lauf_unpoison_alloc_builtin, 0, {
    auto addr = value.as_address;
    if (auto alloc = process->get_allocation(addr))
        alloc->lifetime = lauf::vm_allocation::poisoned;
    else
        LAUF_BUILTIN_OPERATION_PANIC("invalid address");
})

