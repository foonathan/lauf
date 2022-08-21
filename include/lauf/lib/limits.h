// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_LIB_LIMITS_H_INCLUDED
#define LAUF_LIB_LIMITS_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_runtime_builtin         lauf_runtime_builtin;
typedef struct lauf_runtime_builtin_library lauf_runtime_builtin_library;

extern const lauf_runtime_builtin_library lauf_lib_limits;

/// Calls `lauf_runtime_set_step_limit()`.
/// Signature: limit:uint => _
extern const lauf_runtime_builtin lauf_lib_limits_set_step_limit;

/// Calls `lauf_runtime_increment_step()`.
/// Signature: _ => _
extern const lauf_runtime_builtin lauf_lib_limits_step;

LAUF_HEADER_END

#endif // LAUF_LIB_LIMITS_H_INCLUDED

