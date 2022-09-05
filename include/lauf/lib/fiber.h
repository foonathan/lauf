// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_LIB_FIBER_H_INCLUDED
#define LAUF_LIB_FIBER_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_runtime_builtin         lauf_runtime_builtin;
typedef struct lauf_runtime_builtin_library lauf_runtime_builtin_library;

extern const lauf_runtime_builtin_library lauf_lib_fiber;

//// Calls `lauf_runtime_create_fiber()`.
///
/// Signature: fn:function_address => handle:fiber
extern const lauf_runtime_builtin lauf_lib_fiber_create;

/// Calls `lauf_runtime_destroy_fiber()`.
///
/// Signature: handle:fiber => _
extern const lauf_runtime_builtin lauf_lib_fiber_destroy;

/// Calls `lauf_runtime_get_current_fiber()`.
///
/// Signature: _ => handle:fiber
extern const lauf_runtime_builtin lauf_lib_fiber_current;

/// Calls `lauf_runtime_get_fiber_parent()` on the current fiber.
///
/// Signature: _ => parent_handle:fiber
extern const lauf_runtime_builtin lauf_lib_fiber_parent;

/// Returns whether the specified fiber is done.
///
/// Signature: handle:fiber => is_done:bool
extern const lauf_runtime_builtin lauf_lib_fiber_done;

LAUF_HEADER_END

#endif // LAUF_LIB_FIBER_H_INCLUDED

