// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib/limits.h>

#include <lauf/runtime/builtin.h>
#include <lauf/runtime/process.h>
#include <lauf/runtime/value.h>

LAUF_RUNTIME_BUILTIN(lauf_lib_limits_set_step_limit, 1, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY,
                     "set_step_limit", nullptr)
{
    auto new_limit = vstack_ptr[0].as_uint;
    if (new_limit == 0)
        return lauf_runtime_panic(process, "cannot remove step limit");

    if (!lauf_runtime_set_step_limit(process, new_limit))
        return lauf_runtime_panic(process, "cannot increase step limit");

    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_limits_step, 0, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY, "step",
                     &lauf_lib_limits_set_step_limit)
{
    if (!lauf_runtime_increment_step(process))
        return lauf_runtime_panic(process, "step limit exceeded");

    // Note that if the panic recovers (via `lauf.test.assert_panic`), the process now
    // has an unlimited step limit.
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

const lauf_runtime_builtin_library lauf_lib_limits
    = {"lauf.limits", &lauf_lib_limits_step, nullptr};

