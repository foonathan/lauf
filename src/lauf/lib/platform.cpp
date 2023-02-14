// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib/platform.h>

#include <lauf/runtime/builtin.h>
#include <lauf/runtime/value.h>

LAUF_RUNTIME_BUILTIN(lauf_lib_platform_vm, 0, 1,
                     LAUF_RUNTIME_BUILTIN_NO_PANIC | LAUF_RUNTIME_BUILTIN_NO_PROCESS, "vm", nullptr)
{
    --vstack_ptr;
    vstack_ptr[0].as_uint = 1;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_platform_qbe, 0, 1,
                     LAUF_RUNTIME_BUILTIN_NO_PANIC | LAUF_RUNTIME_BUILTIN_NO_PROCESS, "qbe",
                     &lauf_lib_platform_vm)
{
    --vstack_ptr;
    vstack_ptr[0].as_uint = 0;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

const lauf_runtime_builtin_library lauf_lib_platform
    = {"lauf.platform", &lauf_lib_platform_qbe, nullptr};

