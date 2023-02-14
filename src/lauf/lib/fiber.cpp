// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib/fiber.h>

#include <lauf/runtime/builtin.h>
#include <lauf/runtime/memory.h>
#include <lauf/runtime/process.h>
#include <lauf/runtime/value.h>

LAUF_RUNTIME_BUILTIN(lauf_lib_fiber_create, 1, 1, LAUF_RUNTIME_BUILTIN_DEFAULT, "create", nullptr)
{
    auto address = vstack_ptr[0].as_function_address;

    auto fn = lauf_runtime_get_function_ptr_any(process, address);
    if (LAUF_UNLIKELY(fn == nullptr))
        return lauf_runtime_panic(process, "invalid function address");

    auto fiber               = lauf_runtime_create_fiber(process, fn);
    vstack_ptr[0].as_address = lauf_runtime_get_fiber_handle(fiber);

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_fiber_destroy, 1, 0, LAUF_RUNTIME_BUILTIN_DEFAULT, "destroy",
                     &lauf_lib_fiber_create)
{
    auto handle = vstack_ptr[0].as_address;
    ++vstack_ptr;

    auto fiber = lauf_runtime_get_fiber_ptr(process, handle);
    if (LAUF_UNLIKELY(fiber == nullptr))
        return lauf_runtime_panic(process, "invalid fiber handle");

    if (LAUF_UNLIKELY(!lauf_runtime_destroy_fiber(process, fiber)))
        return false;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_fiber_current, 0, 1, LAUF_RUNTIME_BUILTIN_NO_PANIC, "current",
                     &lauf_lib_fiber_destroy)
{
    auto fiber = lauf_runtime_get_current_fiber(process);

    --vstack_ptr;
    vstack_ptr[0].as_address = lauf_runtime_get_fiber_handle(fiber);

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_fiber_parent, 0, 1, LAUF_RUNTIME_BUILTIN_NO_PANIC, "parent",
                     &lauf_lib_fiber_current)
{
    auto fiber  = lauf_runtime_get_current_fiber(process);
    auto parent = lauf_runtime_get_fiber_parent(process, fiber);

    --vstack_ptr;
    if (parent != nullptr)
        vstack_ptr[0].as_address = lauf_runtime_get_fiber_handle(parent);
    else
        vstack_ptr[0].as_address = lauf_runtime_address_null;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_fiber_done, 1, 1, LAUF_RUNTIME_BUILTIN_DEFAULT, "done",
                     &lauf_lib_fiber_parent)
{
    auto handle = vstack_ptr[0].as_address;
    auto fiber  = lauf_runtime_get_fiber_ptr(process, handle);
    if (LAUF_UNLIKELY(fiber == nullptr))
        return lauf_runtime_panic(process, "invalid fiber handle");

    auto status           = lauf_runtime_get_fiber_status(fiber);
    vstack_ptr[0].as_uint = status == LAUF_RUNTIME_FIBER_DONE ? 1 : 0;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

const lauf_runtime_builtin_library lauf_lib_fiber = {"lauf.fiber", &lauf_lib_fiber_done, nullptr};

