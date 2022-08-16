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

const lauf_runtime_builtin_library lauf_lib_memory = {"lauf.memory", &lauf_lib_memory_unpoison};

