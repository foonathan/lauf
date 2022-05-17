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

namespace
{
#define LAUF_BUILTIN_FN(Name)                                                                      \
    bool Name(lauf_vm_instruction* ip, lauf_value* stack_ptr, void* frame_ptr, lauf_vm vm)

#define LAUF_BUILTIN_DISPATCH                                                                      \
    LAUF_TAIL_CALL return lauf_builtin_dispatch(ip, stack_ptr, frame_ptr, vm)

LAUF_BUILTIN_FN(sadd_return)
{
    auto lhs = stack_ptr[1].as_sint;
    auto rhs = stack_ptr[0].as_sint;

    stack_ptr[0].as_uint = __builtin_add_overflow(lhs, rhs, &stack_ptr[1].as_sint) ? 1 : 0;
    LAUF_BUILTIN_DISPATCH;
}
LAUF_BUILTIN_FN(sadd_panic)
{
    auto lhs = stack_ptr[1].as_sint;
    auto rhs = stack_ptr[0].as_sint;

    if (__builtin_add_overflow(lhs, rhs, &stack_ptr[1].as_sint))
        return lauf_builtin_panic(vm, ip, frame_ptr, "integer overflow");

    ++stack_ptr;
    LAUF_BUILTIN_DISPATCH;
}
LAUF_BUILTIN_FN(sadd_wrap)
{
    auto lhs = stack_ptr[1].as_sint;
    auto rhs = stack_ptr[0].as_sint;

    __builtin_add_overflow(lhs, rhs, &stack_ptr[1].as_sint);

    ++stack_ptr;
    LAUF_BUILTIN_DISPATCH;
}
LAUF_BUILTIN_FN(sadd_sat)
{
    auto lhs = stack_ptr[1].as_sint;
    auto rhs = stack_ptr[0].as_sint;

    if (__builtin_add_overflow(lhs, rhs, &stack_ptr[1].as_sint))
    {
        if (rhs < 0)
            stack_ptr[1].as_sint = lauf_value_sint_min;
        else
            stack_ptr[1].as_sint = lauf_value_sint_max;
    }

    ++stack_ptr;
    LAUF_BUILTIN_DISPATCH;
}

LAUF_BUILTIN_FN(ssub_return)
{
    auto lhs = stack_ptr[1].as_sint;
    auto rhs = stack_ptr[0].as_sint;

    stack_ptr[0].as_uint = __builtin_sub_overflow(lhs, rhs, &stack_ptr[1].as_sint) ? 1 : 0;

    LAUF_BUILTIN_DISPATCH;
}
LAUF_BUILTIN_FN(ssub_panic)
{
    auto lhs = stack_ptr[1].as_sint;
    auto rhs = stack_ptr[0].as_sint;

    if (__builtin_sub_overflow(lhs, rhs, &stack_ptr[1].as_sint))
        return lauf_builtin_panic(vm, ip, frame_ptr, "integer overflow");

    ++stack_ptr;
    LAUF_BUILTIN_DISPATCH;
}
LAUF_BUILTIN_FN(ssub_wrap)
{
    auto lhs = stack_ptr[1].as_sint;
    auto rhs = stack_ptr[0].as_sint;

    __builtin_sub_overflow(lhs, rhs, &stack_ptr[1].as_sint);

    ++stack_ptr;
    LAUF_BUILTIN_DISPATCH;
}
LAUF_BUILTIN_FN(ssub_sat)
{
    auto lhs = stack_ptr[1].as_sint;
    auto rhs = stack_ptr[0].as_sint;

    if (__builtin_sub_overflow(lhs, rhs, &stack_ptr[1].as_sint))
    {
        if (rhs < 0)
            stack_ptr[1].as_sint = lauf_value_sint_max;
        else
            stack_ptr[1].as_sint = lauf_value_sint_min;
    }

    ++stack_ptr;
    LAUF_BUILTIN_DISPATCH;
}

LAUF_BUILTIN_FN(smul_return)
{
    auto lhs = stack_ptr[1].as_sint;
    auto rhs = stack_ptr[0].as_sint;

    stack_ptr[0].as_uint = __builtin_mul_overflow(lhs, rhs, &stack_ptr[1].as_sint) ? 1 : 0;

    LAUF_BUILTIN_DISPATCH;
}
LAUF_BUILTIN_FN(smul_panic)
{
    auto lhs = stack_ptr[1].as_sint;
    auto rhs = stack_ptr[0].as_sint;

    if (__builtin_mul_overflow(lhs, rhs, &stack_ptr[1].as_sint))
        return lauf_builtin_panic(vm, ip, frame_ptr, "integer overflow");

    ++stack_ptr;
    LAUF_BUILTIN_DISPATCH;
}
LAUF_BUILTIN_FN(smul_wrap)
{
    auto lhs = stack_ptr[1].as_sint;
    auto rhs = stack_ptr[0].as_sint;

    __builtin_mul_overflow(lhs, rhs, &stack_ptr[1].as_sint);

    ++stack_ptr;
    LAUF_BUILTIN_DISPATCH;
}
LAUF_BUILTIN_FN(smul_sat)
{
    auto lhs = stack_ptr[1].as_sint;
    auto rhs = stack_ptr[0].as_sint;

    if (__builtin_mul_overflow(lhs, rhs, &stack_ptr[1].as_sint))
    {
        if ((rhs < 0) != (lhs < 0))
            stack_ptr[1].as_sint = lauf_value_sint_min;
        else
            stack_ptr[1].as_sint = lauf_value_sint_max;
    }

    ++stack_ptr;
    LAUF_BUILTIN_DISPATCH;
}

LAUF_BUILTIN_FN(uadd_return)
{
    auto lhs = stack_ptr[1].as_uint;
    auto rhs = stack_ptr[0].as_uint;

    stack_ptr[0].as_uint = __builtin_add_overflow(lhs, rhs, &stack_ptr[1].as_uint) ? 1 : 0;

    LAUF_BUILTIN_DISPATCH;
}
LAUF_BUILTIN_FN(uadd_panic)
{
    auto lhs = stack_ptr[1].as_uint;
    auto rhs = stack_ptr[0].as_uint;

    if (__builtin_add_overflow(lhs, rhs, &stack_ptr[1].as_uint))
        return lauf_builtin_panic(vm, ip, frame_ptr, "integer overflow");

    ++stack_ptr;
    LAUF_BUILTIN_DISPATCH;
}
LAUF_BUILTIN_FN(uadd_wrap)
{
    auto lhs = stack_ptr[1].as_uint;
    auto rhs = stack_ptr[0].as_uint;

    __builtin_add_overflow(lhs, rhs, &stack_ptr[1].as_uint);

    ++stack_ptr;
    LAUF_BUILTIN_DISPATCH;
}
LAUF_BUILTIN_FN(uadd_sat)
{
    auto lhs = stack_ptr[1].as_uint;
    auto rhs = stack_ptr[0].as_uint;

    if (__builtin_add_overflow(lhs, rhs, &stack_ptr[1].as_uint))
        stack_ptr[1].as_uint = lauf_value_uint_max;

    ++stack_ptr;
    LAUF_BUILTIN_DISPATCH;
}

LAUF_BUILTIN_FN(usub_return)
{
    auto lhs = stack_ptr[1].as_uint;
    auto rhs = stack_ptr[0].as_uint;

    stack_ptr[0].as_uint = __builtin_sub_overflow(lhs, rhs, &stack_ptr[1].as_uint) ? 1 : 0;

    LAUF_BUILTIN_DISPATCH;
}
LAUF_BUILTIN_FN(usub_panic)
{
    auto lhs = stack_ptr[1].as_uint;
    auto rhs = stack_ptr[0].as_uint;

    if (__builtin_sub_overflow(lhs, rhs, &stack_ptr[1].as_uint))
        return lauf_builtin_panic(vm, ip, frame_ptr, "integer overflow");

    ++stack_ptr;
    LAUF_BUILTIN_DISPATCH;
}
LAUF_BUILTIN_FN(usub_wrap)
{
    auto lhs = stack_ptr[1].as_uint;
    auto rhs = stack_ptr[0].as_uint;

    __builtin_sub_overflow(lhs, rhs, &stack_ptr[1].as_uint);

    ++stack_ptr;
    LAUF_BUILTIN_DISPATCH;
}
LAUF_BUILTIN_FN(usub_sat)
{
    auto lhs = stack_ptr[1].as_uint;
    auto rhs = stack_ptr[0].as_uint;

    if (__builtin_sub_overflow(lhs, rhs, &stack_ptr[1].as_uint))
        stack_ptr[1].as_uint = 0;

    ++stack_ptr;
    LAUF_BUILTIN_DISPATCH;
}

LAUF_BUILTIN_FN(umul_return)
{
    auto lhs = stack_ptr[1].as_uint;
    auto rhs = stack_ptr[0].as_uint;

    stack_ptr[0].as_uint = __builtin_mul_overflow(lhs, rhs, &stack_ptr[1].as_uint) ? 1 : 0;

    LAUF_BUILTIN_DISPATCH;
}
LAUF_BUILTIN_FN(umul_panic)
{
    auto lhs = stack_ptr[1].as_uint;
    auto rhs = stack_ptr[0].as_uint;

    if (__builtin_mul_overflow(lhs, rhs, &stack_ptr[1].as_uint))
        return lauf_builtin_panic(vm, ip, frame_ptr, "integer overflow");

    ++stack_ptr;
    LAUF_BUILTIN_DISPATCH;
}
LAUF_BUILTIN_FN(umul_wrap)
{
    auto lhs = stack_ptr[1].as_uint;
    auto rhs = stack_ptr[0].as_uint;

    __builtin_mul_overflow(lhs, rhs, &stack_ptr[1].as_uint);

    ++stack_ptr;
    LAUF_BUILTIN_DISPATCH;
}
LAUF_BUILTIN_FN(umul_sat)
{
    auto lhs = stack_ptr[1].as_uint;
    auto rhs = stack_ptr[0].as_uint;

    if (__builtin_mul_overflow(lhs, rhs, &stack_ptr[1].as_uint))
        stack_ptr[1].as_uint = lauf_value_uint_max;

    ++stack_ptr;
    LAUF_BUILTIN_DISPATCH;
}
} // namespace

lauf_builtin lauf_sadd_builtin(lauf_integer_overflow overflow)
{
    switch (overflow)
    {
    case LAUF_INTEGER_OVERFLOW_RETURN:
        return {{2, 2}, &sadd_return};
    case LAUF_INTEGER_OVERFLOW_PANIC:
        return {{2, 1}, &sadd_panic};
    case LAUF_INTEGER_OVERFLOW_WRAP:
        return {{2, 1}, &sadd_wrap};
    case LAUF_INTEGER_OVERFLOW_SAT:
        return {{2, 1}, &sadd_sat};
    }
}

lauf_builtin lauf_ssub_builtin(lauf_integer_overflow overflow)
{
    switch (overflow)
    {
    case LAUF_INTEGER_OVERFLOW_RETURN:
        return {{2, 2}, &ssub_return};
    case LAUF_INTEGER_OVERFLOW_PANIC:
        return {{2, 1}, &ssub_panic};
    case LAUF_INTEGER_OVERFLOW_WRAP:
        return {{2, 1}, &ssub_wrap};
    case LAUF_INTEGER_OVERFLOW_SAT:
        return {{2, 1}, &ssub_sat};
    }
}

lauf_builtin lauf_smul_builtin(lauf_integer_overflow overflow)
{
    switch (overflow)
    {
    case LAUF_INTEGER_OVERFLOW_RETURN:
        return {{2, 2}, &smul_return};
    case LAUF_INTEGER_OVERFLOW_PANIC:
        return {{2, 1}, &smul_panic};
    case LAUF_INTEGER_OVERFLOW_WRAP:
        return {{2, 1}, &smul_wrap};
    case LAUF_INTEGER_OVERFLOW_SAT:
        return {{2, 1}, &smul_sat};
    }
}

lauf_builtin lauf_uadd_builtin(lauf_integer_overflow overflow)
{
    switch (overflow)
    {
    case LAUF_INTEGER_OVERFLOW_RETURN:
        return {{2, 2}, &uadd_return};
    case LAUF_INTEGER_OVERFLOW_PANIC:
        return {{2, 1}, &uadd_panic};
    case LAUF_INTEGER_OVERFLOW_WRAP:
        return {{2, 1}, &uadd_wrap};
    case LAUF_INTEGER_OVERFLOW_SAT:
        return {{2, 1}, &uadd_sat};
    }
}

lauf_builtin lauf_usub_builtin(lauf_integer_overflow overflow)
{
    switch (overflow)
    {
    case LAUF_INTEGER_OVERFLOW_RETURN:
        return {{2, 2}, &usub_return};
    case LAUF_INTEGER_OVERFLOW_PANIC:
        return {{2, 1}, &usub_panic};
    case LAUF_INTEGER_OVERFLOW_WRAP:
        return {{2, 1}, &usub_wrap};
    case LAUF_INTEGER_OVERFLOW_SAT:
        return {{2, 1}, &usub_sat};
    }
}

lauf_builtin lauf_umul_builtin(lauf_integer_overflow overflow)
{
    switch (overflow)
    {
    case LAUF_INTEGER_OVERFLOW_RETURN:
        return {{2, 2}, &umul_return};
    case LAUF_INTEGER_OVERFLOW_PANIC:
        return {{2, 1}, &umul_panic};
    case LAUF_INTEGER_OVERFLOW_WRAP:
        return {{2, 1}, &umul_wrap};
    case LAUF_INTEGER_OVERFLOW_SAT:
        return {{2, 1}, &umul_sat};
    }
}

//=== comparison ===//
namespace
{
LAUF_BUILTIN_FN(scmp)
{
    auto lhs = stack_ptr[1].as_sint;
    auto rhs = stack_ptr[0].as_sint;

    if (lhs < rhs)
        stack_ptr[1].as_sint = -1;
    else if (lhs == rhs)
        stack_ptr[1].as_sint = 0;
    else
        stack_ptr[1].as_sint = 1;

    ++stack_ptr;
    LAUF_BUILTIN_DISPATCH;
}

LAUF_BUILTIN_FN(ucmp)
{
    auto lhs = stack_ptr[1].as_uint;
    auto rhs = stack_ptr[0].as_uint;

    if (lhs < rhs)
        stack_ptr[1].as_sint = -1;
    else if (lhs == rhs)
        stack_ptr[1].as_sint = 0;
    else
        stack_ptr[1].as_sint = 1;

    ++stack_ptr;
    LAUF_BUILTIN_DISPATCH;
}
} // namespace

lauf_builtin lauf_scmp_builtin(void)
{
    return {{2, 1}, &scmp};
}

lauf_builtin lauf_ucmp_builtin(void)
{
    return {{2, 1}, &ucmp};
}

