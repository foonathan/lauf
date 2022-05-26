// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib/int.h>

#include <new>

//=== types ===//
namespace
{
constexpr lauf_type_data sint_type
    = {LAUF_NATIVE_LAYOUT_OF(lauf_value_sint), 1,
       [](const void* object_address, size_t) {
           lauf_value result;
           result.as_sint = *static_cast<const lauf_value_sint*>(object_address);
           return result;
       },
       [](void* object_address, size_t, lauf_value value) {
           ::new (object_address) lauf_value_sint(value.as_sint);
       }};

constexpr lauf_type_data uint_type
    = {LAUF_NATIVE_LAYOUT_OF(lauf_value_sint), 1,
       [](const void* object_address, size_t) {
           lauf_value result;
           result.as_uint = *static_cast<const lauf_value_uint*>(object_address);
           return result;
       },
       [](void* object_address, size_t, lauf_value value) {
           ::new (object_address) lauf_value_uint(value.as_uint);
       }};
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
        return lauf_builtin_panic(process, ip, frame_ptr, "integer overflow");
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
        return lauf_builtin_panic(process, ip, frame_ptr, "integer overflow");
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
        return lauf_builtin_panic(process, ip, frame_ptr, "integer overflow");
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
        return lauf_builtin_panic(process, ip, frame_ptr, "integer overflow");
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
        return lauf_builtin_panic(process, ip, frame_ptr, "integer overflow");
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
        return lauf_builtin_panic(process, ip, frame_ptr, "integer overflow");
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
    if (lhs.as_sint < rhs.as_sint)
        result->as_sint = -1;
    else if (lhs.as_sint == rhs.as_sint)
        result->as_sint = 0;
    else
        result->as_sint = 1;
})
LAUF_BUILTIN_BINARY_OPERATION(lauf_ucmp_builtin, 1, {
    if (lhs.as_uint < rhs.as_uint)
        result->as_sint = -1;
    else if (lhs.as_uint == rhs.as_uint)
        result->as_sint = 0;
    else
        result->as_sint = 1;
})

