// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib/test.h>

#include <lauf/lib/debug.h>
#include <lauf/runtime/builtin.h>
#include <lauf/runtime/process.h>
#include <lauf/runtime/value.h>

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

const lauf_runtime_builtin_library lauf_lib_test = {"lauf.test", &lauf_lib_test_assert_eq};

