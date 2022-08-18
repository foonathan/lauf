// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_LIB_LIMITS_H_INCLUDED
#define LAUF_LIB_LIMITS_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_runtime_builtin         lauf_runtime_builtin;
typedef struct lauf_runtime_builtin_library lauf_runtime_builtin_library;

extern const lauf_runtime_builtin_library lauf_lib_limits;

/// Limits the number of execution steps that can be taken before the process panics.
///
/// This has no effect, unless `lauf_lib_limitis_step[s]` is called, which modifies the limit.
/// The limit cannot be increased beyond the limit provided in the VM config.
/// It also reaches the existing steps taken.
///
/// Signature: limit:uint => _
extern const lauf_runtime_builtin lauf_lib_limits_set_step_limit;

/// Increments the step count by one.
///
/// If this reaches the step limit, panics.
/// It needs to be manually inserted during codegen (e.g. once per function and loop iteratation)
/// for the step limit to work. If the step limit is unlimited, does nothing.
///
/// Signature: _ => _
extern const lauf_runtime_builtin lauf_lib_limits_step;

LAUF_HEADER_END

#endif // LAUF_LIB_LIMITS_H_INCLUDED

