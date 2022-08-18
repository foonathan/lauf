// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib/memory.h>

#include <lauf/runtime/builtin.h>
#include <lauf/runtime/process.hpp>
#include <lauf/runtime/value.h>

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_poison, 1, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY, "poison", nullptr)
{
    auto address = vstack_ptr[0].as_address;
    ++vstack_ptr;

    auto alloc = process->get_allocation(address);
    if (alloc == nullptr || !lauf::is_usable(alloc->status))
        return lauf_runtime_panic(process, "invalid address");
    alloc->status = lauf::allocation_status::poison;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_unpoison, 1, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY, "unpoison",
                     &lauf_lib_memory_poison)
{
    auto address = vstack_ptr[0].as_address;
    ++vstack_ptr;

    auto alloc = process->get_allocation(address);
    if (alloc == nullptr || alloc->status != lauf::allocation_status::poison)
        return lauf_runtime_panic(process, "invalid address");
    alloc->status = lauf::allocation_status::allocated;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_split, 1, 2, LAUF_RUNTIME_BUILTIN_VM_ONLY, "split",
                     &lauf_lib_memory_unpoison)
{
    auto addr = vstack_ptr[0].as_address;

    auto alloc = process->get_allocation(addr);
    if (alloc == nullptr || !lauf::is_usable(alloc->status) || addr.offset >= alloc->size)
        return lauf_runtime_panic(process, "invalid address");

    // We create a new allocation as a copy, but with modified pointer and size.
    // If the original allocation was unsplit or the last split, the new allocation is the end of
    // the allocation. Otherwise, it is somewhere in the middle.
    auto new_alloc = *alloc;
    new_alloc.ptr  = static_cast<unsigned char*>(new_alloc.ptr) + addr.offset;
    new_alloc.size -= addr.offset;
    new_alloc.split = alloc->split == lauf::allocation_split::unsplit
                              || alloc->split == lauf::allocation_split::split_last
                          ? lauf::allocation_split::split_last
                          : lauf::allocation_split::split_middle;
    auto addr2      = process->add_allocation(new_alloc);

    // We now modify the original allocation, by shrinking it.
    // If the original allocation was unsplit or the first split, it is the first split.
    // Otherwise, it is somewhere in the middle.
    alloc->size  = addr.offset;
    alloc->split = alloc->split == lauf::allocation_split::unsplit
                           || alloc->split == lauf::allocation_split::split_first
                       ? lauf::allocation_split::split_first
                       : lauf::allocation_split::split_middle;
    auto addr1   = lauf_runtime_address{addr.allocation, addr.generation, 0};

    --vstack_ptr;
    vstack_ptr[1].as_address = addr1;
    vstack_ptr[0].as_address = addr2;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_merge, 2, 1, LAUF_RUNTIME_BUILTIN_VM_ONLY, "merge",
                     &lauf_lib_memory_split)
{
    auto addr1 = vstack_ptr[1].as_address;
    auto addr2 = vstack_ptr[0].as_address;

    auto alloc1 = process->get_allocation(addr1);
    auto alloc2 = process->get_allocation(addr2);
    if (alloc1 == nullptr
        || alloc2 == nullptr
        // Allocations must be usable.
        || !lauf::is_usable(alloc1->status)
        || !lauf::is_usable(alloc2->status)
        // Allocations must be split.
        || alloc1->split == lauf::allocation_split::unsplit
        || alloc2->split == lauf::allocation_split::unsplit
        // And they must be adjacent.
        || static_cast<unsigned char*>(alloc1->ptr) + alloc1->size != alloc2->ptr)
        return lauf_runtime_panic(process, "invalid address");

    // alloc1 grows to cover alloc2.
    alloc1->size += alloc2->size;

    // Since alloc1 precedes alloc2, the following split configuration are possible:
    // 1. (first, mid)
    // 2. (first, last)
    // 3. (mid, mid)
    // 4. (mid, last)
    //
    // In case 2, we merged everything back and are unsplit.
    // In case 4, we create a bigger last split.
    if (alloc2->split == lauf::allocation_split::split_last)
    {
        if (alloc1->split == lauf::allocation_split::split_first)
            alloc1->split = lauf::allocation_split::unsplit; // case 2
        else
            alloc1->split = lauf::allocation_split::split_last; // case 4
    }

    // We don't need alloc2 anymore.
    alloc2->status = lauf::allocation_status::freed;

    ++vstack_ptr;
    vstack_ptr[0].as_address = addr1;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_addr_to_int, 1, 2, LAUF_RUNTIME_BUILTIN_DEFAULT, "addr_to_int",
                     &lauf_lib_memory_merge)
{
    auto addr = vstack_ptr[0].as_address;

    auto alloc = process->get_allocation(addr);
    if (alloc == nullptr || addr.offset >= alloc->size)
        return lauf_runtime_panic(process, "invalid address");

    auto ptr = static_cast<unsigned char*>(alloc->ptr) + addr.offset;
    // The provenance is an address with invalid offset that still keeps it alive during garbage
    // collection.
    auto provenance = lauf_runtime_address{addr.allocation, addr.generation, alloc->size};

    --vstack_ptr;
    vstack_ptr[1].as_address = provenance;
    vstack_ptr[0].as_uint    = reinterpret_cast<std::uintptr_t>(ptr);
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_int_to_addr, 2, 1, LAUF_RUNTIME_BUILTIN_DEFAULT, "int_to_addr",
                     &lauf_lib_memory_addr_to_int)
{
    auto provenance = vstack_ptr[1].as_address;
    auto ptr        = reinterpret_cast<unsigned char*>(vstack_ptr[0].as_uint); // NOLINT

    auto alloc = process->get_allocation(provenance);
    if (alloc == nullptr || provenance.offset != alloc->size)
        return lauf_runtime_panic(process, "invalid provenance");

    auto offset = ptr - static_cast<unsigned char*>(alloc->ptr);
    if (offset < 0 || offset >= alloc->size)
        return lauf_runtime_panic(process, "invalid address");

    auto addr
        = lauf_runtime_address{provenance.allocation, provenance.generation, std::uint32_t(offset)};
    ++vstack_ptr;
    vstack_ptr[0].as_address = addr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_addr_add, 2, 1, LAUF_RUNTIME_BUILTIN_NO_PANIC, "addr_add",
                     &lauf_lib_memory_int_to_addr)
{
    auto addr   = vstack_ptr[1].as_address;
    auto offset = vstack_ptr[0].as_sint;

    lauf_sint result;
    auto      overflow = __builtin_add_overflow(lauf_sint(addr.offset), offset, &result);
    if (overflow || result < 0 || result > UINT32_MAX)
        result = UINT32_MAX;

    ++vstack_ptr;
    vstack_ptr[0].as_address = {addr.allocation, addr.generation, std::uint32_t(result)};
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_addr_sub, 2, 1, LAUF_RUNTIME_BUILTIN_NO_PANIC, "addr_sub",
                     &lauf_lib_memory_addr_add)
{
    auto addr   = vstack_ptr[1].as_address;
    auto offset = vstack_ptr[0].as_sint;

    lauf_sint result;
    auto      overflow = __builtin_sub_overflow(lauf_sint(addr.offset), offset, &result);
    if (overflow || result < 0 || result > UINT32_MAX)
        result = UINT32_MAX;

    ++vstack_ptr;
    vstack_ptr[0].as_address = {addr.allocation, addr.generation, std::uint32_t(result)};
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_addr_distance, 2, 1, LAUF_RUNTIME_BUILTIN_DEFAULT,
                     "addr_distance", &lauf_lib_memory_addr_sub)
{
    auto lhs = vstack_ptr[1].as_address;
    auto rhs = vstack_ptr[0].as_address;

    if (lhs.allocation != rhs.allocation || lhs.generation != rhs.generation)
        return lauf_runtime_panic(process, "addresses are from different allocations");

    auto distance = lauf_sint(lhs.offset) - lauf_sint(rhs.offset);

    ++vstack_ptr;
    vstack_ptr[0].as_sint = distance;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

const lauf_runtime_builtin_library lauf_lib_memory
    = {"lauf.memory", &lauf_lib_memory_addr_distance};

