// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_FUNCTION_H_INCLUDED
#define LAUF_FUNCTION_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

union lauf_Value;

typedef struct lauf_FunctionSignature
{
    /// The number of values the function pops from the stack as parameters.
    uint8_t input_count;
    /// The number of values the function pushes on the stack as return values.
    uint8_t output_count;
} lauf_FunctionSignature;

//=== function ===//
typedef struct lauf_FunctionImpl* lauf_Function;

void lauf_function_destroy(lauf_Function fn);

const char* lauf_function_name(lauf_Function fn);

lauf_FunctionSignature lauf_function_signature(lauf_Function fn);

//=== builtin function ===//
typedef union lauf_Value* lauf_BuiltinFunctionCallback(union lauf_Value* stack_ptr);

typedef struct lauf_BuiltinFunction
{
    lauf_FunctionSignature        signature;
    lauf_BuiltinFunctionCallback* impl;
} lauf_BuiltinFunction;

#define LAUF_BUILTIN_FUNCTION(Name, InputCount, OutputCount)                                       \
    static union lauf_Value*   Name##_impl(union lauf_Value* stack_ptr);                           \
    const lauf_BuiltinFunction Name = {{InputCount, OutputCount}, &Name##_impl};                   \
    static union lauf_Value*   Name##_impl(union lauf_Value* stack_ptr)

#define LAUF_BUILTIN_UNARY_OP(Name)                                                                \
    static union lauf_Value Name##_op_impl(union lauf_Value value);                                \
    LAUF_BUILTIN_FUNCTION(Name, 1, 1)                                                              \
    {                                                                                              \
        stack_ptr[-1] = Name##_op_impl(stack_ptr[-1]);                                             \
        return stack_ptr;                                                                          \
    }                                                                                              \
    static union lauf_Value Name##_op_impl(union lauf_Value value)

#define LAUF_BUILTIN_BINARY_OP(Name)                                                               \
    static union lauf_Value Name##_op_impl(union lauf_Value lhs, union lauf_Value rhs);            \
    LAUF_BUILTIN_FUNCTION(Name, 2, 1)                                                              \
    {                                                                                              \
        stack_ptr[-2] = Name##_op_impl(stack_ptr[-2], stack_ptr[-1]);                              \
        return stack_ptr - 1;                                                                      \
    }                                                                                              \
    static union lauf_Value Name##_op_impl(union lauf_Value lhs, union lauf_Value rhs)

LAUF_HEADER_END

#endif // LAUF_FUNCTION_H_INCLUDED

