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
    /// Operations have the signature: `a b => result did_overflow`
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

lauf_runtime_builtin lauf_lib_int_sadd(lauf_lib_int_overflow overflow);
lauf_runtime_builtin lauf_lib_int_ssub(lauf_lib_int_overflow overflow);
lauf_runtime_builtin lauf_lib_int_smul(lauf_lib_int_overflow overflow);

//=== comparison ===//
extern const lauf_runtime_builtin lauf_lib_int_scmp;
extern const lauf_runtime_builtin lauf_lib_int_ucmp;

LAUF_HEADER_END

#endif // LAUF_LIB_INT_H_INCLUDED

