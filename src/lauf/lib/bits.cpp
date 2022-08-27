// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib/bits.h>

#include <lauf/runtime/builtin.h>
#include <lauf/runtime/process.h>
#include <lauf/runtime/value.h>
#include <lauf/vm_execute.hpp> // So builtin dispatch is inlined.

namespace
{
constexpr auto no_panic_flags = LAUF_RUNTIME_BUILTIN_NO_PANIC | LAUF_RUNTIME_BUILTIN_NO_PROCESS
                                | LAUF_RUNTIME_BUILTIN_CONSTANT_FOLD;
} // namespace

LAUF_RUNTIME_BUILTIN(lauf_lib_bits_and, 2, 1, no_panic_flags, "and", nullptr)
{
    auto lhs = vstack_ptr[1].as_uint;
    auto rhs = vstack_ptr[0].as_uint;
    ++vstack_ptr;

    vstack_ptr[0].as_uint = lhs & rhs;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_bits_or, 2, 1, no_panic_flags, "or", &lauf_lib_bits_and)
{
    auto lhs = vstack_ptr[1].as_uint;
    auto rhs = vstack_ptr[0].as_uint;
    ++vstack_ptr;

    vstack_ptr[0].as_uint = lhs | rhs;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_bits_xor, 2, 1, no_panic_flags, "xor", &lauf_lib_bits_or)
{
    auto lhs = vstack_ptr[1].as_uint;
    auto rhs = vstack_ptr[0].as_uint;
    ++vstack_ptr;

    vstack_ptr[0].as_uint = lhs ^ rhs;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

const lauf_runtime_builtin_library lauf_lib_bits = {"lauf.bits", &lauf_lib_bits_xor};

