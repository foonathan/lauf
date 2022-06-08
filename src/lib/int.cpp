// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib/int.h>

#include <lauf/impl/builtin.hpp>
#include <new>

//=== types ===//
namespace
{
LAUF_NATIVE_SINGLE_VALUE_TYPE(sint_type, lauf_value_sint, as_sint);
LAUF_NATIVE_SINGLE_VALUE_TYPE(uint_type, lauf_value_uint, as_uint);
} // namespace

lauf_type lauf_native_sint_type(void)
{
    return &sint_type;
}

lauf_type lauf_native_uint_type(void)
{
    return &uint_type;
}

//=== arithmetic builtins ===//
#ifndef __GNUC__
#    error "unsupported compiler"
#endif

#define LAUF_MAKE_ARITHMETIC_BUILTIN(Name)                                                         \
    lauf_builtin lauf_##Name##_builtin(lauf_integer_overflow overflow)                             \
    {                                                                                              \
        switch (overflow)                                                                          \
        {                                                                                          \
        case LAUF_INTEGER_OVERFLOW_RETURN:                                                         \
            return Name##_return();                                                                \
        case LAUF_INTEGER_OVERFLOW_PANIC:                                                          \
            return Name##_panic();                                                                 \
        case LAUF_INTEGER_OVERFLOW_WRAP:                                                           \
            return Name##_wrap();                                                                  \
        case LAUF_INTEGER_OVERFLOW_SAT:                                                            \
            return Name##_sat();                                                                   \
        }                                                                                          \
    }

LAUF_BUILTIN_BINARY_OPERATION(sadd_return, 2, {
    result[0].as_uint
        = __builtin_add_overflow(lhs.as_sint, rhs.as_sint, &result[1].as_sint) ? 1 : 0;
})
LAUF_BUILTIN_BINARY_OPERATION(sadd_panic, 1, {
    if (__builtin_add_overflow(lhs.as_sint, rhs.as_sint, &result->as_sint))
        LAUF_BUILTIN_OPERATION_PANIC("integer overflow");
})
LAUF_BUILTIN_BINARY_OPERATION(sadd_wrap, 1, {
    __builtin_add_overflow(lhs.as_sint, rhs.as_sint, &result->as_sint);
})
LAUF_BUILTIN_BINARY_OPERATION(sadd_sat, 1, {
    if (__builtin_add_overflow(lhs.as_sint, rhs.as_sint, &result->as_sint))
    {
        if (rhs.as_sint < 0)
            result->as_sint = lauf_value_sint_min;
        else
            result->as_sint = lauf_value_sint_max;
    }
})
LAUF_MAKE_ARITHMETIC_BUILTIN(sadd)

LAUF_BUILTIN_BINARY_OPERATION(ssub_return, 2, {
    result[0].as_uint
        = __builtin_sub_overflow(lhs.as_sint, rhs.as_sint, &result[1].as_sint) ? 1 : 0;
})
LAUF_BUILTIN_BINARY_OPERATION(ssub_panic, 1, {
    if (__builtin_sub_overflow(lhs.as_sint, rhs.as_sint, &result->as_sint))
        LAUF_BUILTIN_OPERATION_PANIC("integer overflow");
})
LAUF_BUILTIN_BINARY_OPERATION(ssub_wrap, 1, {
    __builtin_sub_overflow(lhs.as_sint, rhs.as_sint, &result->as_sint);
})
LAUF_BUILTIN_BINARY_OPERATION(ssub_sat, 1, {
    if (__builtin_sub_overflow(lhs.as_sint, rhs.as_sint, &result->as_sint))
    {
        if (rhs.as_sint < 0)
            result->as_sint = lauf_value_sint_max;
        else
            result->as_sint = lauf_value_sint_min;
    }
})
LAUF_MAKE_ARITHMETIC_BUILTIN(ssub)

LAUF_BUILTIN_BINARY_OPERATION(smul_return, 2, {
    result[0].as_uint
        = __builtin_mul_overflow(lhs.as_sint, rhs.as_sint, &result[1].as_sint) ? 1 : 0;
})
LAUF_BUILTIN_BINARY_OPERATION(smul_panic, 1, {
    if (__builtin_mul_overflow(lhs.as_sint, rhs.as_sint, &result->as_sint))
        LAUF_BUILTIN_OPERATION_PANIC("integer overflow");
})
LAUF_BUILTIN_BINARY_OPERATION(smul_wrap, 1, {
    __builtin_mul_overflow(lhs.as_sint, rhs.as_sint, &result->as_sint);
})
LAUF_BUILTIN_BINARY_OPERATION(smul_sat, 1, {
    if (__builtin_mul_overflow(lhs.as_sint, rhs.as_sint, &result->as_sint))
    {
        if ((rhs.as_sint < 0) != (lhs.as_sint < 0))
            result->as_sint = lauf_value_sint_min;
        else
            result->as_sint = lauf_value_sint_max;
    }
})
LAUF_MAKE_ARITHMETIC_BUILTIN(smul)

LAUF_BUILTIN_BINARY_OPERATION(uadd_return, 2, {
    result[0].as_uint
        = __builtin_add_overflow(lhs.as_uint, rhs.as_uint, &result[1].as_uint) ? 1 : 0;
})
LAUF_BUILTIN_BINARY_OPERATION(uadd_panic, 1, {
    if (__builtin_add_overflow(lhs.as_uint, rhs.as_uint, &result->as_uint))
        LAUF_BUILTIN_OPERATION_PANIC("integer overflow");
})
LAUF_BUILTIN_BINARY_OPERATION(uadd_wrap, 1, {
    __builtin_add_overflow(lhs.as_uint, rhs.as_uint, &result->as_uint);
})
LAUF_BUILTIN_BINARY_OPERATION(uadd_sat, 1, {
    if (__builtin_add_overflow(lhs.as_uint, rhs.as_uint, &result->as_uint))
        result->as_uint = lauf_value_uint_max;
})
LAUF_MAKE_ARITHMETIC_BUILTIN(uadd)

LAUF_BUILTIN_BINARY_OPERATION(usub_return, 2, {
    result[0].as_uint
        = __builtin_sub_overflow(lhs.as_uint, rhs.as_sint, &result[1].as_uint) ? 1 : 0;
})
LAUF_BUILTIN_BINARY_OPERATION(usub_panic, 1, {
    if (__builtin_sub_overflow(lhs.as_uint, rhs.as_uint, &result->as_uint))
        LAUF_BUILTIN_OPERATION_PANIC("integer overflow");
})
LAUF_BUILTIN_BINARY_OPERATION(usub_wrap, 1, {
    __builtin_sub_overflow(lhs.as_uint, rhs.as_uint, &result->as_uint);
})
LAUF_BUILTIN_BINARY_OPERATION(usub_sat, 1, {
    if (__builtin_sub_overflow(lhs.as_uint, rhs.as_uint, &result->as_uint))
        result->as_uint = 0;
})
LAUF_MAKE_ARITHMETIC_BUILTIN(usub)

LAUF_BUILTIN_BINARY_OPERATION(umul_return, 2, {
    result[0].as_uint
        = __builtin_mul_overflow(lhs.as_uint, rhs.as_uint, &result[1].as_uint) ? 1 : 0;
})
LAUF_BUILTIN_BINARY_OPERATION(umul_panic, 1, {
    if (__builtin_mul_overflow(lhs.as_uint, rhs.as_uint, &result->as_uint))
        LAUF_BUILTIN_OPERATION_PANIC("integer overflow");
})
LAUF_BUILTIN_BINARY_OPERATION(umul_wrap, 1, {
    __builtin_mul_overflow(lhs.as_uint, rhs.as_uint, &result->as_uint);
})
LAUF_BUILTIN_BINARY_OPERATION(umul_sat, 1, {
    if (__builtin_mul_overflow(lhs.as_uint, rhs.as_uint, &result->as_uint))
        result->as_uint = lauf_value_uint_max;
})
LAUF_MAKE_ARITHMETIC_BUILTIN(umul)

//=== comparison ===//
LAUF_BUILTIN_BINARY_OPERATION(lauf_scmp_builtin, 1, {
    result->as_sint = (lhs.as_sint > rhs.as_sint) - (lhs.as_sint < rhs.as_sint);
})
LAUF_BUILTIN_BINARY_OPERATION(lauf_ucmp_builtin, 1, {
    result->as_sint = (lhs.as_uint > rhs.as_uint) - (lhs.as_uint < rhs.as_uint);
})

