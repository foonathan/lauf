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
constexpr auto panic_flags = LAUF_RUNTIME_BUILTIN_NO_PROCESS | LAUF_RUNTIME_BUILTIN_CONSTANT_FOLD;
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

LAUF_RUNTIME_BUILTIN(lauf_lib_bits_shl, 2, 1, panic_flags, "shl", &lauf_lib_bits_xor)
{
    auto x = vstack_ptr[1].as_uint;
    auto n = vstack_ptr[0].as_uint;
    ++vstack_ptr;

    if (LAUF_UNLIKELY(n >= sizeof(lauf_uint) * CHAR_BIT))
        return lauf_runtime_panic(process, "shift amount too big");

    vstack_ptr[0].as_uint = x << n;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_bits_ushr, 2, 1, panic_flags, "ushr", &lauf_lib_bits_shl)
{
    auto x = vstack_ptr[1].as_uint;
    auto n = vstack_ptr[0].as_uint;
    ++vstack_ptr;

    if (LAUF_UNLIKELY(n >= sizeof(lauf_uint) * CHAR_BIT))
        return lauf_runtime_panic(process, "shift amount too big");

    vstack_ptr[0].as_uint = x >> n;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_bits_sshr, 2, 1, panic_flags, "sshr", &lauf_lib_bits_ushr)
{
    auto x = vstack_ptr[1].as_sint;
    auto n = vstack_ptr[0].as_uint;
    ++vstack_ptr;

    if (LAUF_UNLIKELY(n >= sizeof(lauf_sint) * CHAR_BIT))
        return lauf_runtime_panic(process, "shift amount too big");

    static_assert(-1 >> 1 == -1, "compiler does not implement arithmetic right shift");
    vstack_ptr[0].as_sint = x >> n;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

const lauf_runtime_builtin_library lauf_lib_bits = {"lauf.bits", &lauf_lib_bits_sshr, nullptr};

