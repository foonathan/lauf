// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib/fiber.h>

#include <lauf/runtime/builtin.h>
#include <lauf/runtime/memory.h>
#include <lauf/runtime/process.h>
#include <lauf/runtime/value.h>

LAUF_RUNTIME_BUILTIN(lauf_lib_fiber_create, 1, 1, LAUF_RUNTIME_BUILTIN_VM_ONLY, "create", nullptr)
{
    auto address = vstack_ptr[0].as_function_address;

    auto fn = lauf_runtime_get_function_ptr_any(process, address);
    if (LAUF_UNLIKELY(fn == nullptr))
        return lauf_runtime_panic(process, "invalid function address");

    auto fiber               = lauf_runtime_create_fiber(process, fn);
    vstack_ptr[0].as_address = lauf_runtime_get_fiber_handle(fiber);

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_fiber_destroy, 1, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY, "destroy",
                     &lauf_lib_fiber_create)
{
    auto handle = vstack_ptr[0].as_address;
    ++vstack_ptr;

    auto fiber = lauf_runtime_get_fiber_ptr(process, handle);
    if (LAUF_UNLIKELY(fiber == nullptr))
        return lauf_runtime_panic(process, "invalid fiber handle");

    lauf_runtime_destroy_fiber(process, fiber);

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_fiber_current, 0, 1, LAUF_RUNTIME_BUILTIN_VM_ONLY, "current",
                     &lauf_lib_fiber_destroy)
{
    auto fiber = lauf_runtime_get_current_fiber(process);

    --vstack_ptr;
    vstack_ptr[0].as_address = lauf_runtime_get_fiber_handle(fiber);

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

const lauf_runtime_builtin_library lauf_lib_fiber = {"lauf.fiber", &lauf_lib_fiber_current};

