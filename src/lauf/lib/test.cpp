// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib/test.h>

#include <cstring>
#include <lauf/asm/module.h>
#include <lauf/runtime/builtin.h>
#include <lauf/runtime/memory.h>
#include <lauf/runtime/process.h>
#include <lauf/runtime/value.h>
#include <lauf/vm.h>

LAUF_RUNTIME_BUILTIN(lauf_lib_test_dynamic, 1, 1, LAUF_RUNTIME_BUILTIN_NO_PROCESS, "dynamic",
                     nullptr)
{
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(lauf_lib_test_dynamic2, 2, 2, LAUF_RUNTIME_BUILTIN_NO_PROCESS, "dynamic2",
                     &lauf_lib_test_dynamic)
{
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_test_unreachable, 0, 0, LAUF_RUNTIME_BUILTIN_NO_PROCESS,
                     "unreachable", &lauf_lib_test_dynamic2)
{
    (void)ip;
    (void)vstack_ptr;
    (void)frame_ptr;
    return lauf_runtime_panic(process, "unreachable code reached");
}

LAUF_RUNTIME_BUILTIN(lauf_lib_test_assert, 1, 0, LAUF_RUNTIME_BUILTIN_NO_PROCESS, "assert",
                     &lauf_lib_test_unreachable)
{
    auto value = vstack_ptr[0].as_uint;
    ++vstack_ptr;

    if (value != 0)
        LAUF_RUNTIME_BUILTIN_DISPATCH;
    else
        return lauf_runtime_panic(process, "assert failed");
}

LAUF_RUNTIME_BUILTIN(lauf_lib_test_assert_eq, 2, 0, LAUF_RUNTIME_BUILTIN_NO_PROCESS, "assert_eq",
                     &lauf_lib_test_assert)
{
    auto lhs = vstack_ptr[1].as_uint;
    auto rhs = vstack_ptr[0].as_uint;
    vstack_ptr += 2;

    if (lhs == rhs)
        LAUF_RUNTIME_BUILTIN_DISPATCH;
    else
        return lauf_runtime_panic(process, "assert_eq failed");
}

LAUF_RUNTIME_BUILTIN(lauf_lib_test_assert_panic, 2, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY, "assert_panic",
                     &lauf_lib_test_assert_eq)
{
    auto expected_msg = lauf_runtime_get_cstr(process, vstack_ptr[0].as_address);
    auto fn = lauf_runtime_get_function_ptr(process, vstack_ptr[1].as_function_address, {0, 0});
    vstack_ptr += 2;

    if (fn == nullptr)
        return lauf_runtime_panic(process, "invalid function");

    // We temporarily replace the panic handler with one that simply remembers the message.
    auto                            vm = lauf_runtime_get_vm(process);
    static thread_local const char* panic_msg;
    auto handler = lauf_vm_set_panic_handler(vm, [](lauf_runtime_process*, const char* msg) {
        panic_msg = msg;
    });

    auto did_not_panic = lauf_runtime_call(process, fn, nullptr, nullptr);

    lauf_vm_set_panic_handler(vm, handler);

    if (did_not_panic)
        return lauf_runtime_panic(process, "assert_panic failed: no panic");
    else if (expected_msg == nullptr && panic_msg != nullptr)
        return lauf_runtime_panic(process, "assert_panic failed: did not expect message");
    else if (expected_msg != nullptr && std::strcmp(expected_msg, panic_msg) != 0)
        return lauf_runtime_panic(process, "assert_panic failed: different message");
    else
        LAUF_RUNTIME_BUILTIN_DISPATCH;
}

const lauf_runtime_builtin_library lauf_lib_test
    = {"lauf.test", &lauf_lib_test_assert_panic, nullptr};

