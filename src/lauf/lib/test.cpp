// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib/test.h>

#include <lauf/asm/module.h>
#include <lauf/lib/debug.h>
#include <lauf/runtime/builtin.h>
#include <lauf/runtime/process.h>
#include <lauf/runtime/value.h>
#include <lauf/vm.hpp>

const lauf_runtime_builtin_function lauf_lib_test_unreachable = {
    [](lauf_runtime_process* p, const lauf_runtime_value*, lauf_runtime_value*) {
        return lauf_runtime_panic(p, "unreachable code reached");
    },
    0,
    0,
    LAUF_RUNTIME_BUILTIN_DEFAULT,
    "unreachable",
    nullptr,
};

const lauf_runtime_builtin_function lauf_lib_test_assert = {
    [](lauf_runtime_process* p, const lauf_runtime_value* input, lauf_runtime_value*) {
        if (input[0].as_uint == 0)
            return true;

        lauf_lib_debug_print.impl(p, input, nullptr);
        return lauf_runtime_panic(p, "assert failed");
    },
    1,
    0,
    LAUF_RUNTIME_BUILTIN_DEFAULT,
    "assert",
    &lauf_lib_test_unreachable,
};

const lauf_runtime_builtin_function lauf_lib_test_assert_eq = {
    [](lauf_runtime_process* p, const lauf_runtime_value* input, lauf_runtime_value*) {
        if (input[0].as_uint == input[1].as_uint)
            return true;

        lauf_lib_debug_print.impl(p, input + 1, nullptr);
        lauf_lib_debug_print.impl(p, input, nullptr);
        return lauf_runtime_panic(p, "assert_eq failed");
    },
    2,
    0,
    LAUF_RUNTIME_BUILTIN_DEFAULT,
    "assert_eq",
    &lauf_lib_test_assert,
};

const lauf_runtime_builtin_function lauf_lib_test_assert_panic = {
    [](lauf_runtime_process* p, const lauf_runtime_value* input, lauf_runtime_value*) {
        auto expected_msg = lauf_runtime_get_cstr(p, input[0].as_address);

        auto fn = lauf_runtime_get_function_ptr(p, input[1].as_function_address, {0, 0});
        if (fn == nullptr)
            return lauf_runtime_panic(p, "invalid function");

        // We temporarily replace the panic handler with one that simply remembers the message.
        auto                            handler = p->vm->panic_handler;
        static thread_local const char* panic_msg;
        p->vm->panic_handler = [](lauf_runtime_process*, const char* msg) { panic_msg = msg; };

        auto did_not_panic = lauf_runtime_call(p, fn, nullptr, nullptr);

        p->vm->panic_handler = handler;

        if (did_not_panic)
            return lauf_runtime_panic(p, "assert_panic failed: no panic");
        else if (expected_msg == nullptr && panic_msg != nullptr)
            return lauf_runtime_panic(p, "assert_panic failed: did not expect message");
        else if (expected_msg != nullptr && std::strcmp(expected_msg, panic_msg) != 0)
            return lauf_runtime_panic(p, "assert_panic failed: different message");

        // It did panic correctly, so we swallow it and continue.
        return true;
    },
    2,
    0,
    LAUF_RUNTIME_BUILTIN_VM_ONLY,
    "assert_panic",
    &lauf_lib_test_assert_eq,
};

const lauf_runtime_builtin_library lauf_lib_test = {"lauf.test", &lauf_lib_test_assert_panic};

