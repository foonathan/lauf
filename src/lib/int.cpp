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

//=== signed builtins ===//
#ifndef __GNUC__
#    error "unsupported compiler"
#endif

lauf_builtin lauf_sadd_builtin(lauf_integer_overflow overflow)
{
    switch (overflow)
    {
    case LAUF_INTEGER_OVERFLOW_REPORT:
        return {{2, 2}, [](lauf_value* stack_ptr) {
                    auto lhs = stack_ptr[-2].as_sint;
                    auto rhs = stack_ptr[-1].as_sint;

                    stack_ptr[-1].as_uint
                        = __builtin_add_overflow(lhs, rhs, &stack_ptr[-2].as_sint) ? 1 : 0;

                    return stack_ptr;
                }};

    case LAUF_INTEGER_OVERFLOW_WRAP:
        return {{2, 1}, [](lauf_value* stack_ptr) {
                    auto lhs = stack_ptr[-2].as_sint;
                    auto rhs = stack_ptr[-1].as_sint;

                    __builtin_add_overflow(lhs, rhs, &stack_ptr[-2].as_sint);

                    --stack_ptr;
                    return stack_ptr;
                }};

    case LAUF_INTEGER_OVERFLOW_SAT:
        return {{2, 1}, [](lauf_value* stack_ptr) {
                    auto lhs = stack_ptr[-2].as_sint;
                    auto rhs = stack_ptr[-1].as_sint;

                    if (__builtin_add_overflow(lhs, rhs, &stack_ptr[-2].as_sint))
                    {
                        if (rhs < 0)
                            stack_ptr[-2].as_sint = lauf_value_sint_min;
                        else
                            stack_ptr[-2].as_sint = lauf_value_sint_max;
                    }

                    --stack_ptr;
                    return stack_ptr;
                }};
    }
}

lauf_builtin lauf_ssub_builtin(lauf_integer_overflow overflow)
{
    switch (overflow)
    {
    case LAUF_INTEGER_OVERFLOW_REPORT:
        return {{2, 2}, [](lauf_value* stack_ptr) {
                    auto lhs = stack_ptr[-2].as_sint;
                    auto rhs = stack_ptr[-1].as_sint;

                    stack_ptr[-1].as_uint
                        = __builtin_sub_overflow(lhs, rhs, &stack_ptr[-2].as_sint) ? 1 : 0;

                    return stack_ptr;
                }};

    case LAUF_INTEGER_OVERFLOW_WRAP:
        return {{2, 1}, [](lauf_value* stack_ptr) {
                    auto lhs = stack_ptr[-2].as_sint;
                    auto rhs = stack_ptr[-1].as_sint;

                    __builtin_sub_overflow(lhs, rhs, &stack_ptr[-2].as_sint);

                    --stack_ptr;
                    return stack_ptr;
                }};

    case LAUF_INTEGER_OVERFLOW_SAT:
        return {{2, 1}, [](lauf_value* stack_ptr) {
                    auto lhs = stack_ptr[-2].as_sint;
                    auto rhs = stack_ptr[-1].as_sint;

                    if (__builtin_sub_overflow(lhs, rhs, &stack_ptr[-2].as_sint))
                    {
                        if (rhs < 0)
                            stack_ptr[-2].as_sint = lauf_value_sint_max;
                        else
                            stack_ptr[-2].as_sint = lauf_value_sint_min;
                    }

                    --stack_ptr;
                    return stack_ptr;
                }};
    }
}

lauf_builtin lauf_smul_builtin(lauf_integer_overflow overflow)
{
    switch (overflow)
    {
    case LAUF_INTEGER_OVERFLOW_REPORT:
        return {{2, 2}, [](lauf_value* stack_ptr) {
                    auto lhs = stack_ptr[-2].as_sint;
                    auto rhs = stack_ptr[-1].as_sint;

                    stack_ptr[-1].as_uint
                        = __builtin_mul_overflow(lhs, rhs, &stack_ptr[-2].as_sint) ? 1 : 0;

                    return stack_ptr;
                }};

    case LAUF_INTEGER_OVERFLOW_WRAP:
        return {{2, 1}, [](lauf_value* stack_ptr) {
                    auto lhs = stack_ptr[-2].as_sint;
                    auto rhs = stack_ptr[-1].as_sint;

                    __builtin_mul_overflow(lhs, rhs, &stack_ptr[-2].as_sint);

                    --stack_ptr;
                    return stack_ptr;
                }};

    case LAUF_INTEGER_OVERFLOW_SAT:
        return {{2, 1}, [](lauf_value* stack_ptr) {
                    auto lhs = stack_ptr[-2].as_sint;
                    auto rhs = stack_ptr[-1].as_sint;

                    if (__builtin_mul_overflow(lhs, rhs, &stack_ptr[-2].as_sint))
                    {
                        if ((rhs < 0) != (lhs < 0))
                            stack_ptr[-2].as_sint = lauf_value_sint_min;
                        else
                            stack_ptr[-2].as_sint = lauf_value_sint_max;
                    }

                    --stack_ptr;
                    return stack_ptr;
                }};
    }
}

lauf_builtin lauf_uadd_builtin(lauf_integer_overflow overflow)
{
    switch (overflow)
    {
    case LAUF_INTEGER_OVERFLOW_REPORT:
        return {{2, 2}, [](lauf_value* stack_ptr) {
                    auto lhs = stack_ptr[-2].as_uint;
                    auto rhs = stack_ptr[-1].as_uint;

                    stack_ptr[-1].as_uint
                        = __builtin_add_overflow(lhs, rhs, &stack_ptr[-2].as_uint) ? 1 : 0;

                    return stack_ptr;
                }};

    case LAUF_INTEGER_OVERFLOW_WRAP:
        return {{2, 1}, [](lauf_value* stack_ptr) {
                    auto lhs = stack_ptr[-2].as_uint;
                    auto rhs = stack_ptr[-1].as_uint;

                    __builtin_add_overflow(lhs, rhs, &stack_ptr[-2].as_uint);

                    --stack_ptr;
                    return stack_ptr;
                }};

    case LAUF_INTEGER_OVERFLOW_SAT:
        return {{2, 1}, [](lauf_value* stack_ptr) {
                    auto lhs = stack_ptr[-2].as_uint;
                    auto rhs = stack_ptr[-1].as_uint;

                    if (__builtin_add_overflow(lhs, rhs, &stack_ptr[-2].as_uint))
                        stack_ptr[-2].as_uint = lauf_value_uint_max;

                    --stack_ptr;
                    return stack_ptr;
                }};
    }
}

lauf_builtin lauf_usub_builtin(lauf_integer_overflow overflow)
{
    switch (overflow)
    {
    case LAUF_INTEGER_OVERFLOW_REPORT:
        return {{2, 2}, [](lauf_value* stack_ptr) {
                    auto lhs = stack_ptr[-2].as_uint;
                    auto rhs = stack_ptr[-1].as_uint;

                    stack_ptr[-1].as_uint
                        = __builtin_sub_overflow(lhs, rhs, &stack_ptr[-2].as_uint) ? 1 : 0;

                    return stack_ptr;
                }};

    case LAUF_INTEGER_OVERFLOW_WRAP:
        return {{2, 1}, [](lauf_value* stack_ptr) {
                    auto lhs = stack_ptr[-2].as_uint;
                    auto rhs = stack_ptr[-1].as_uint;

                    __builtin_sub_overflow(lhs, rhs, &stack_ptr[-2].as_uint);

                    --stack_ptr;
                    return stack_ptr;
                }};

    case LAUF_INTEGER_OVERFLOW_SAT:
        return {{2, 1}, [](lauf_value* stack_ptr) {
                    auto lhs = stack_ptr[-2].as_uint;
                    auto rhs = stack_ptr[-1].as_uint;

                    if (__builtin_sub_overflow(lhs, rhs, &stack_ptr[-2].as_uint))
                        stack_ptr[-2].as_uint = 0;

                    --stack_ptr;
                    return stack_ptr;
                }};
    }
}

lauf_builtin lauf_umul_builtin(lauf_integer_overflow overflow)
{
    switch (overflow)
    {
    case LAUF_INTEGER_OVERFLOW_REPORT:
        return {{2, 2}, [](lauf_value* stack_ptr) {
                    auto lhs = stack_ptr[-2].as_uint;
                    auto rhs = stack_ptr[-1].as_uint;

                    stack_ptr[-1].as_uint
                        = __builtin_mul_overflow(lhs, rhs, &stack_ptr[-2].as_uint) ? 1 : 0;

                    return stack_ptr;
                }};

    case LAUF_INTEGER_OVERFLOW_WRAP:
        return {{2, 1}, [](lauf_value* stack_ptr) {
                    auto lhs = stack_ptr[-2].as_uint;
                    auto rhs = stack_ptr[-1].as_uint;

                    __builtin_mul_overflow(lhs, rhs, &stack_ptr[-2].as_uint);

                    --stack_ptr;
                    return stack_ptr;
                }};

    case LAUF_INTEGER_OVERFLOW_SAT:
        return {{2, 1}, [](lauf_value* stack_ptr) {
                    auto lhs = stack_ptr[-2].as_uint;
                    auto rhs = stack_ptr[-1].as_uint;

                    if (__builtin_mul_overflow(lhs, rhs, &stack_ptr[-2].as_uint))
                        stack_ptr[-2].as_uint = lauf_value_uint_max;

                    --stack_ptr;
                    return stack_ptr;
                }};
    }
}

//=== comparison ===//
lauf_builtin lauf_scmp_builtin(void)
{
    return {{2, 1}, [](lauf_value* stack_ptr) {
                auto lhs = stack_ptr[-2].as_sint;
                auto rhs = stack_ptr[-1].as_sint;

                if (lhs < rhs)
                    stack_ptr[-2].as_sint = -1;
                else if (lhs == rhs)
                    stack_ptr[-2].as_sint = 0;
                else
                    stack_ptr[-2].as_sint = 1;

                --stack_ptr;
                return stack_ptr;
            }};
}

lauf_builtin lauf_ucmp_builtin(void)
{
    return {{2, 1}, [](lauf_value* stack_ptr) {
                auto lhs = stack_ptr[-2].as_uint;
                auto rhs = stack_ptr[-1].as_uint;

                if (lhs < rhs)
                    stack_ptr[-2].as_sint = -1;
                else if (lhs == rhs)
                    stack_ptr[-2].as_sint = 0;
                else
                    stack_ptr[-2].as_sint = 1;

                --stack_ptr;
                return stack_ptr;
            }};
}

