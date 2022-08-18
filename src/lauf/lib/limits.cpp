// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib/limits.h>

#include <lauf/runtime/builtin.h>
#include <lauf/runtime/process.hpp>
#include <lauf/runtime/value.h>
#include <lauf/vm.hpp>

LAUF_RUNTIME_BUILTIN(lauf_lib_limits_set_step_limit, 1, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY,
                     "set_step_limit", nullptr)
{
    auto new_limit = vstack_ptr[0].as_uint;
    if (new_limit == 0)
        return lauf_runtime_panic(process, "cannot remove step limit");

    auto vm_limit = process->vm->step_limit;
    if (vm_limit != 0 && new_limit > vm_limit)
        return lauf_runtime_panic(process, "cannot increase step limit");

    process->remaining_steps = new_limit;
    assert(process->remaining_steps != 0);

    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_limits_step, 0, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY, "step",
                     &lauf_lib_limits_set_step_limit)
{
    // If the remaining steps are zero, we have no limit.
    if (process->remaining_steps > 0)
    {
        --process->remaining_steps;
        if (process->remaining_steps == 0)
            return lauf_runtime_panic(process, "step limit exceeded");

        // Note that if the panic recovers (via `lauf.test.assert_panic`), the process now
        // has an unlimited step limit.
    }

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

const lauf_runtime_builtin_library lauf_lib_limits = {"lauf.limits", &lauf_lib_limits_step};

