// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_LIB_INT_H_INCLUDED
#define LAUF_LIB_INT_H_INCLUDED

#include <lauf/builtin.h>
#include <lauf/config.h>
#include <lauf/type.h>

//=== types ===//
lauf_type lauf_native_sint_type(void);
lauf_type lauf_native_uint_type(void);

//=== arithmetic ===//
typedef enum lauf_integer_overflow
{
    /// Gives a boolean result indicating whether overflow occurred.
    LAUF_INTEGER_OVERFLOW_REPORT,
    /// Wraps on integer overflow.
    LAUF_INTEGER_OVERFLOW_WRAP,
    /// Saturates on integer overflow.
    LAUF_INTEGER_OVERFLOW_SAT,
} lauf_integer_overflow;

lauf_builtin lauf_sadd_builtin(lauf_integer_overflow overflow);
lauf_builtin lauf_ssub_builtin(lauf_integer_overflow overflow);
lauf_builtin lauf_smul_builtin(lauf_integer_overflow overflow);

lauf_builtin lauf_uadd_builtin(lauf_integer_overflow overflow);
lauf_builtin lauf_usub_builtin(lauf_integer_overflow overflow);
lauf_builtin lauf_umul_builtin(lauf_integer_overflow overflow);

//=== comparison ===//
lauf_builtin lauf_scmp_builtin(void);
lauf_builtin lauf_ucmp_builtin(void);

#endif // LAUF_LIB_INT_H_INCLUDED

