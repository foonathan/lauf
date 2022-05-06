// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_BUILTIN_H_INCLUDED
#define LAUF_BUILTIN_H_INCLUDED

#include <lauf/config.h>
#include <lauf/module.h>

LAUF_HEADER_START

typedef union lauf_value* lauf_builtin_function(union lauf_value* stack_ptr);

typedef struct lauf_builtin
{
    lauf_signature         signature;
    lauf_builtin_function* impl;
} lauf_builtin;

#define LAUF_BUILTIN(Name, InputCount, OutputCount)                                                \
    static union lauf_value* Name##_impl(union lauf_value* vstack_ptr);                            \
    const lauf_builtin       Name = {{InputCount, OutputCount}, &Name##_impl};                     \
    static union lauf_value* Name##_impl(union lauf_value* vstack_ptr)

#define LAUF_BUILTIN_UNARY_OP(Name)                                                                \
    static union lauf_value Name##_op_impl(union lauf_value value);                                \
    LAUF_BUILTIN(Name, 1, 1)                                                                       \
    {                                                                                              \
        vstack_ptr[-1] = Name##_op_impl(vstack_ptr[-1]);                                           \
        return vstack_ptr;                                                                         \
    }                                                                                              \
    static union lauf_value Name##_op_impl(union lauf_value value)

#define LAUF_BUILTIN_BINARY_OP(Name)                                                               \
    static union lauf_value Name##_op_impl(union lauf_value lhs, union lauf_value rhs);            \
    LAUF_BUILTIN(Name, 2, 1)                                                                       \
    {                                                                                              \
        vstack_ptr[-2] = Name##_op_impl(vstack_ptr[-2], vstack_ptr[-1]);                           \
        return vstack_ptr - 1;                                                                     \
    }                                                                                              \
    static union lauf_value Name##_op_impl(union lauf_value lhs, union lauf_value rhs)

LAUF_HEADER_END

#endif // LAUF_BUILTIN_H_INCLUDED

