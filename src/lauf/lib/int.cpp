// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib/int.h>

#include <lauf/runtime/builtin.h>
#include <lauf/runtime/process.h>
#include <lauf/runtime/value.h>
#include <lauf/vm_execute.hpp> // So builtin dispatch is inlined.

#define LAUF_MAKE_ARITHMETIC_BUILTIN(Name)                                                         \
    lauf_runtime_builtin lauf_lib_int_##Name(lauf_lib_int_overflow overflow)                       \
    {                                                                                              \
        switch (overflow)                                                                          \
        {                                                                                          \
        case LAUF_LIB_INT_OVERFLOW_FLAG:                                                           \
            return Name##_flag;                                                                    \
        case LAUF_LIB_INT_OVERFLOW_WRAP:                                                           \
            return Name##_wrap;                                                                    \
        case LAUF_LIB_INT_OVERFLOW_SAT:                                                            \
            return Name##_sat;                                                                     \
        case LAUF_LIB_INT_OVERFLOW_PANIC:                                                          \
            return Name##_panic;                                                                   \
        }                                                                                          \
    }

namespace
{
constexpr auto no_panic_flags = LAUF_RUNTIME_BUILTIN_NO_PANIC | LAUF_RUNTIME_BUILTIN_NO_PROCESS
                                | LAUF_RUNTIME_BUILTIN_CONSTANT_FOLD;
constexpr auto panic_flags = LAUF_RUNTIME_BUILTIN_NO_PROCESS | LAUF_RUNTIME_BUILTIN_CONSTANT_FOLD;
} // namespace

namespace
{
LAUF_RUNTIME_BUILTIN(sadd_flag, 2, 2, no_panic_flags, "sadd_flag", nullptr)
{
    auto overflow         = __builtin_add_overflow(vstack_ptr[1].as_sint, vstack_ptr[0].as_sint,
                                                   &vstack_ptr[1].as_sint);
    vstack_ptr[0].as_uint = overflow ? 1 : 0;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(sadd_wrap, 2, 1, no_panic_flags, "sadd_wrap", &sadd_flag)
{
    __builtin_add_overflow(vstack_ptr[1].as_sint, vstack_ptr[0].as_sint, &vstack_ptr[1].as_sint);
    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(sadd_sat, 2, 1, no_panic_flags, "sadd_sat", &sadd_wrap)
{
    auto overflow = __builtin_add_overflow(vstack_ptr[1].as_sint, vstack_ptr[0].as_sint,
                                           &vstack_ptr[1].as_sint);
    if (overflow)
    {
        if (vstack_ptr[0].as_sint < 0)
            vstack_ptr[1].as_sint = INT64_MIN;
        else
            vstack_ptr[1].as_sint = INT64_MAX;
    }
    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(sadd_panic, 2, 1, panic_flags, "sadd_panic", &sadd_sat)
{
    auto overflow = __builtin_add_overflow(vstack_ptr[1].as_sint, vstack_ptr[0].as_sint,
                                           &vstack_ptr[1].as_sint);
    if (overflow)
        return lauf_runtime_panic(process, "integer overflow");
    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
} // namespace

LAUF_MAKE_ARITHMETIC_BUILTIN(sadd)

namespace
{
LAUF_RUNTIME_BUILTIN(ssub_flag, 2, 2, no_panic_flags, "ssub_flag", &sadd_panic)
{
    auto overflow         = __builtin_sub_overflow(vstack_ptr[1].as_sint, vstack_ptr[0].as_sint,
                                                   &vstack_ptr[1].as_sint);
    vstack_ptr[0].as_uint = overflow ? 1 : 0;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(ssub_wrap, 2, 1, no_panic_flags, "ssub_wrap", &ssub_flag)
{
    __builtin_sub_overflow(vstack_ptr[1].as_sint, vstack_ptr[0].as_sint, &vstack_ptr[1].as_sint);
    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(ssub_sat, 2, 1, no_panic_flags, "ssub_sat", &ssub_wrap)
{
    auto overflow = __builtin_sub_overflow(vstack_ptr[1].as_sint, vstack_ptr[0].as_sint,
                                           &vstack_ptr[1].as_sint);
    if (overflow)
    {
        if (vstack_ptr[0].as_sint < 0)
            vstack_ptr[1].as_sint = INT64_MAX;
        else
            vstack_ptr[1].as_sint = INT64_MIN;
    }
    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(ssub_panic, 2, 1, panic_flags, "ssub_panic", &ssub_sat)
{
    auto overflow = __builtin_sub_overflow(vstack_ptr[1].as_sint, vstack_ptr[0].as_sint,
                                           &vstack_ptr[1].as_sint);
    if (overflow)
        return lauf_runtime_panic(process, "integer overflow");
    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
} // namespace

LAUF_MAKE_ARITHMETIC_BUILTIN(ssub)

namespace
{
LAUF_RUNTIME_BUILTIN(smul_flag, 2, 2, no_panic_flags, "smul_flag", &ssub_panic)
{
    auto overflow         = __builtin_mul_overflow(vstack_ptr[1].as_sint, vstack_ptr[0].as_sint,
                                                   &vstack_ptr[1].as_sint);
    vstack_ptr[0].as_uint = overflow ? 1 : 0;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(smul_wrap, 2, 1, no_panic_flags, "smul_wrap", &smul_flag)
{
    __builtin_mul_overflow(vstack_ptr[1].as_sint, vstack_ptr[0].as_sint, &vstack_ptr[1].as_sint);
    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(smul_sat, 2, 1, no_panic_flags, "smul_sat", &smul_wrap)
{
    auto different_signs = (vstack_ptr[1].as_sint < 0) != (vstack_ptr[0].as_sint < 0);
    auto overflow        = __builtin_mul_overflow(vstack_ptr[1].as_sint, vstack_ptr[0].as_sint,
                                                  &vstack_ptr[1].as_sint);
    if (overflow)
    {
        if (different_signs)
            vstack_ptr[1].as_sint = INT64_MIN;
        else
            vstack_ptr[1].as_sint = INT64_MAX;
    }
    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(smul_panic, 2, 1, panic_flags, "smul_panic", &smul_sat)
{
    auto overflow = __builtin_mul_overflow(vstack_ptr[1].as_sint, vstack_ptr[0].as_sint,
                                           &vstack_ptr[1].as_sint);
    if (overflow)
        return lauf_runtime_panic(process, "integer overflow");
    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
} // namespace

LAUF_MAKE_ARITHMETIC_BUILTIN(smul)

namespace
{
LAUF_RUNTIME_BUILTIN(uadd_flag, 2, 2, no_panic_flags, "uadd_flag", &smul_panic)
{
    auto overflow         = __builtin_add_overflow(vstack_ptr[1].as_uint, vstack_ptr[0].as_uint,
                                                   &vstack_ptr[1].as_uint);
    vstack_ptr[0].as_uint = overflow ? 1 : 0;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(uadd_wrap, 2, 1, no_panic_flags, "uadd_wrap", &uadd_flag)
{
    __builtin_add_overflow(vstack_ptr[1].as_uint, vstack_ptr[0].as_uint, &vstack_ptr[1].as_uint);
    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(uadd_sat, 2, 1, no_panic_flags, "uadd_sat", &uadd_wrap)
{
    auto overflow = __builtin_add_overflow(vstack_ptr[1].as_uint, vstack_ptr[0].as_uint,
                                           &vstack_ptr[1].as_uint);
    if (overflow)
        vstack_ptr[1].as_uint = UINT64_MAX;
    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(uadd_panic, 2, 1, panic_flags, "uadd_panic", &uadd_sat)
{
    auto overflow = __builtin_add_overflow(vstack_ptr[1].as_uint, vstack_ptr[0].as_uint,
                                           &vstack_ptr[1].as_uint);
    if (overflow)
        return lauf_runtime_panic(process, "integer overflow");
    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
} // namespace

LAUF_MAKE_ARITHMETIC_BUILTIN(uadd)

namespace
{
LAUF_RUNTIME_BUILTIN(usub_flag, 2, 2, no_panic_flags, "usub_flag", &uadd_panic)
{
    auto overflow         = __builtin_sub_overflow(vstack_ptr[1].as_uint, vstack_ptr[0].as_uint,
                                                   &vstack_ptr[1].as_uint);
    vstack_ptr[0].as_uint = overflow ? 1 : 0;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(usub_wrap, 2, 1, no_panic_flags, "usub_wrap", &usub_flag)
{
    __builtin_sub_overflow(vstack_ptr[1].as_uint, vstack_ptr[0].as_uint, &vstack_ptr[1].as_uint);
    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(usub_sat, 2, 1, no_panic_flags, "usub_sat", &usub_wrap)
{
    auto overflow = __builtin_sub_overflow(vstack_ptr[1].as_uint, vstack_ptr[0].as_uint,
                                           &vstack_ptr[1].as_uint);
    if (overflow)
        vstack_ptr[1].as_uint = 0;
    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(usub_panic, 2, 1, panic_flags, "usub_panic", &usub_sat)
{
    auto overflow = __builtin_sub_overflow(vstack_ptr[1].as_uint, vstack_ptr[0].as_uint,
                                           &vstack_ptr[1].as_uint);
    if (overflow)
        return lauf_runtime_panic(process, "integer overflow");
    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
} // namespace

LAUF_MAKE_ARITHMETIC_BUILTIN(usub)

namespace
{
LAUF_RUNTIME_BUILTIN(umul_flag, 2, 2, no_panic_flags, "umul_flag", &usub_panic)
{
    auto overflow         = __builtin_mul_overflow(vstack_ptr[1].as_uint, vstack_ptr[0].as_uint,
                                                   &vstack_ptr[1].as_uint);
    vstack_ptr[0].as_uint = overflow ? 1 : 0;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(umul_wrap, 2, 1, no_panic_flags, "umul_wrap", &umul_flag)
{
    __builtin_mul_overflow(vstack_ptr[1].as_uint, vstack_ptr[0].as_uint, &vstack_ptr[1].as_uint);
    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(umul_sat, 2, 1, no_panic_flags, "umul_sat", &umul_wrap)
{
    auto overflow = __builtin_mul_overflow(vstack_ptr[1].as_uint, vstack_ptr[0].as_uint,
                                           &vstack_ptr[1].as_uint);
    if (overflow)
        vstack_ptr[1].as_uint = UINT64_MAX;
    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(umul_panic, 2, 1, panic_flags, "umul_panic", &umul_sat)
{
    auto overflow = __builtin_mul_overflow(vstack_ptr[1].as_uint, vstack_ptr[0].as_uint,
                                           &vstack_ptr[1].as_uint);
    if (overflow)
        return lauf_runtime_panic(process, "integer overflow");
    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
} // namespace

LAUF_MAKE_ARITHMETIC_BUILTIN(umul)

LAUF_RUNTIME_BUILTIN(lauf_lib_int_scmp, 2, 1, no_panic_flags, "scmp", &umul_panic)
{
    vstack_ptr[1].as_sint = int(vstack_ptr[1].as_sint > vstack_ptr[0].as_sint)
                            - int(vstack_ptr[1].as_sint < vstack_ptr[0].as_sint);
    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_int_ucmp, 2, 1, no_panic_flags, "ucmp", &lauf_lib_int_scmp)
{
    vstack_ptr[1].as_sint = int(vstack_ptr[1].as_uint > vstack_ptr[0].as_uint)
                            - int(vstack_ptr[1].as_uint < vstack_ptr[0].as_uint);
    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

namespace
{
// The conversion itself is a no-op, only handle overflow.
LAUF_RUNTIME_BUILTIN(stou_flag, 1, 2, no_panic_flags, "stou_flag", &lauf_lib_int_ucmp)
{
    auto is_negative = vstack_ptr[0].as_sint < 0;
    --vstack_ptr;
    vstack_ptr[0].as_uint = is_negative ? 1 : 0;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(stou_wrap, 1, 1, no_panic_flags, "stou_wrap", &stou_flag)
{
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(stou_sat, 1, 1, no_panic_flags, "stou_sat", &stou_wrap)
{
    if (vstack_ptr[0].as_sint < 0)
        vstack_ptr[0].as_uint = 0;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(stou_panic, 1, 1, panic_flags, "stou_panic", &stou_sat)
{
    if (vstack_ptr[0].as_sint < 0)
        return lauf_runtime_panic(process, "integer overflow");
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(utos_flag, 1, 2, no_panic_flags, "utos_flag", &stou_panic)
{
    auto is_too_big = vstack_ptr[0].as_uint > INT64_MAX;
    --vstack_ptr;
    vstack_ptr[0].as_uint = is_too_big ? 1 : 0;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(utos_wrap, 1, 1, no_panic_flags, "utos_wrap", &utos_flag)
{
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(utos_sat, 1, 1, no_panic_flags, "utos_sat", &utos_wrap)
{
    if (vstack_ptr[0].as_uint > INT64_MAX)
        vstack_ptr[0].as_sint = INT64_MAX;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(utos_panic, 1, 1, panic_flags, "utos_panic", &utos_sat)
{
    if (vstack_ptr[0].as_uint > INT64_MAX)
        return lauf_runtime_panic(process, "integer overflow");
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
} // namespace

LAUF_MAKE_ARITHMETIC_BUILTIN(stou)
LAUF_MAKE_ARITHMETIC_BUILTIN(utos)

namespace
{
LAUF_RUNTIME_BUILTIN(sabs_flag, 1, 2, no_panic_flags, "sabs_flag", &utos_panic)
{
    auto input = vstack_ptr[0].as_sint;
    if (input == INT64_MIN)
    {
        --vstack_ptr;
        vstack_ptr[0].as_uint = 1;
    }
    else if (input < 0)
    {
        vstack_ptr[0].as_sint = -input;
        --vstack_ptr;
        vstack_ptr[0].as_uint = 0;
    }
    else
    {
        --vstack_ptr;
        vstack_ptr[0].as_uint = 0;
    }

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(sabs_wrap, 1, 1, no_panic_flags, "sabs_wrap", &sabs_flag)
{
    auto input = vstack_ptr[0].as_sint;
    if (input < 0 && input != INT64_MIN)
        vstack_ptr[0].as_sint = -input;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(sabs_sat, 1, 1, no_panic_flags, "sabs_sat", &sabs_wrap)
{
    auto input = vstack_ptr[0].as_sint;
    if (input == INT64_MIN)
        vstack_ptr[0].as_sint = INT64_MAX;
    else if (input < 0)
        vstack_ptr[0].as_sint = -input;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(sabs_panic, 1, 1, panic_flags, "sabs_panic", &sabs_sat)
{
    auto input = vstack_ptr[0].as_sint;
    if (input == INT64_MIN)
        return lauf_runtime_panic(process, "integer overflow");
    else if (input < 0)
        vstack_ptr[0].as_sint = -input;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
} // namespace

LAUF_MAKE_ARITHMETIC_BUILTIN(sabs)

LAUF_RUNTIME_BUILTIN(lauf_lib_int_uabs, 1, 1, no_panic_flags, "uabs", &sabs_panic)
{
    auto input = vstack_ptr[0].as_sint;
    if (input == INT64_MIN)
    {
        vstack_ptr[0].as_uint = lauf_uint(INT64_MAX) + 1;
    }
    else if (input < 0)
    {
        vstack_ptr[0].as_uint = lauf_uint(-input);
    }

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

const lauf_runtime_builtin_library lauf_lib_int = {"lauf.int", &lauf_lib_int_uabs};

