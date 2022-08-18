// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_LIB_INT_H_INCLUDED
#define LAUF_LIB_INT_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_runtime_builtin         lauf_runtime_builtin;
typedef struct lauf_runtime_builtin_library lauf_runtime_builtin_library;

extern const lauf_runtime_builtin_library lauf_lib_int;

typedef enum lauf_lib_int_overflow
{
    /// Operations have the signature: `inputs => result did_overflow`
    LAUF_LIB_INT_OVERFLOW_FLAG,
    /// Operations wrap around.
    LAUF_LIB_INT_OVERFLOW_WRAP,
    /// Operations saturate on overflow.
    LAUF_LIB_INT_OVERFLOW_SAT,
    /// Operations panic on overflow.
    LAUF_LIB_INT_OVERFLOW_PANIC,
} lauf_lib_int_overflow;

//=== basic arithmetic ===//
lauf_runtime_builtin lauf_lib_int_sadd(lauf_lib_int_overflow overflow);
lauf_runtime_builtin lauf_lib_int_ssub(lauf_lib_int_overflow overflow);
lauf_runtime_builtin lauf_lib_int_smul(lauf_lib_int_overflow overflow);

lauf_runtime_builtin lauf_lib_int_uadd(lauf_lib_int_overflow overflow);
lauf_runtime_builtin lauf_lib_int_usub(lauf_lib_int_overflow overflow);
lauf_runtime_builtin lauf_lib_int_umul(lauf_lib_int_overflow overflow);

/// Overflow behavior controls what happens on INT_MIN / -1.
/// Division by zero panics.
lauf_runtime_builtin lauf_lib_int_sdiv(lauf_lib_int_overflow overflow);

/// Division by zero panics.
extern const lauf_runtime_builtin lauf_lib_int_udiv;

/// Remainder of division, not modulo (result has same sign ad dividend).
extern const lauf_runtime_builtin lauf_lib_int_srem;
extern const lauf_runtime_builtin lauf_lib_int_urem;

//=== comparison ===//
extern const lauf_runtime_builtin lauf_lib_int_scmp;
extern const lauf_runtime_builtin lauf_lib_int_ucmp;

//=== conversion ===//
/// Signed to unsigned conversion.
lauf_runtime_builtin lauf_lib_int_stou(lauf_lib_int_overflow overflow);
/// Unsigned to signed conversion.
lauf_runtime_builtin lauf_lib_int_utos(lauf_lib_int_overflow overflow);

/// Signed to absolute signed.
lauf_runtime_builtin lauf_lib_sabs(lauf_lib_int_overflow overflow);

/// Signed to absolute unsigned (can't overflow).
extern const lauf_runtime_builtin lauf_lib_uabs;

LAUF_HEADER_END

#endif // LAUF_LIB_INT_H_INCLUDED

