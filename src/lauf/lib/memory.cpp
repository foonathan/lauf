// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib/memory.h>

#include <lauf/runtime/builtin.h>
#include <lauf/runtime/process.hpp>
#include <lauf/runtime/value.h>

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_poison, 1, 0, LAUF_RUNTIME_BUILTIN_DEFAULT, "poison", nullptr)
{
    auto address = vstack_ptr[0].as_address;
    ++vstack_ptr;

    auto alloc = process->get_allocation(address);
    if (alloc == nullptr || alloc->status != lauf::allocation_status::allocated)
        return lauf_runtime_panic(process, "invalid address");
    alloc->status = lauf::allocation_status::poison;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_unpoison, 1, 0, LAUF_RUNTIME_BUILTIN_DEFAULT, "unpoison",
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

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_split, 1, 2, LAUF_RUNTIME_BUILTIN_DEFAULT, "split",
                     &lauf_lib_memory_unpoison)
{
    auto ptr = vstack_ptr[0].as_address;

    auto alloc = process->get_allocation(ptr);
    if (alloc == nullptr || alloc->status != lauf::allocation_status::allocated
        || ptr.offset >= alloc->size)
        return lauf_runtime_panic(process, "invalid address");

    // We create a new allocation as a copy, but with modified pointer and size.
    // If the original allocation was unsplit or the last split, the new allocation is the end of
    // the allocation. Otherwise, it is somewhere in the middle.
    auto new_alloc = *alloc;
    new_alloc.ptr  = static_cast<unsigned char*>(new_alloc.ptr) + ptr.offset;
    new_alloc.size -= ptr.offset;
    new_alloc.split = alloc->split == lauf::allocation_split::unsplit
                              || alloc->split == lauf::allocation_split::split_last
                          ? lauf::allocation_split::split_last
                          : lauf::allocation_split::split_middle;
    auto ptr2       = process->add_allocation(new_alloc);

    // We now modify the original allocation, by shrinking it.
    // If the original allocation was unsplit or the first split, it is the first split.
    // Otherwise, it is somewhere in the middle.
    alloc->size  = ptr.offset;
    alloc->split = alloc->split == lauf::allocation_split::unsplit
                           || alloc->split == lauf::allocation_split::split_first
                       ? lauf::allocation_split::split_first
                       : lauf::allocation_split::split_middle;
    auto ptr1    = lauf_runtime_address{ptr.allocation, ptr.generation, 0};

    --vstack_ptr;
    vstack_ptr[1].as_address = ptr1;
    vstack_ptr[0].as_address = ptr2;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_merge, 2, 1, LAUF_RUNTIME_BUILTIN_DEFAULT, "merge",
                     &lauf_lib_memory_split)
{
    auto ptr1 = vstack_ptr[1].as_address;
    auto ptr2 = vstack_ptr[0].as_address;

    auto alloc1 = process->get_allocation(ptr1);
    auto alloc2 = process->get_allocation(ptr2);
    if (alloc1 == nullptr || alloc2 == nullptr
        || alloc1->status != lauf::allocation_status::allocated
        || alloc2->status != lauf::allocation_status::allocated
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
    vstack_ptr[0].as_address = ptr1;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

const lauf_runtime_builtin_library lauf_lib_memory = {"lauf.memory", &lauf_lib_memory_merge};

