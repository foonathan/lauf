// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_LIB_DEBUG_H_INCLUDED
#define LAUF_LIB_DEBUG_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_runtime_builtin         lauf_runtime_builtin;
typedef struct lauf_runtime_builtin_library lauf_runtime_builtin_library;

extern const lauf_runtime_builtin_library lauf_lib_debug;

/// Prints the top value of the stack for debugging purposes.
///
/// Signature: v => v
extern const lauf_runtime_builtin lauf_lib_debug_print;

/// Prints the entire vstack of the current fiber for debugging purposes.
///
/// Signature: _ => _
extern const lauf_runtime_builtin lauf_lib_debug_print_vstack;

/// Prints a stacktrace of the current fiber.
///
/// Signature: _ => _
extern const lauf_runtime_builtin lauf_lib_debug_print_cstack;

/// Prints a stacktrace of all fibers.
///
/// Signature: _ => _
extern const lauf_runtime_builtin lauf_lib_debug_print_all_cstacks;

/// Breaks into the debugger.
///
/// Signature: _ => _
extern const lauf_runtime_builtin lauf_lib_debug_break;

/// Reads hex bytes from stdin and pushes them on top of the stack.
///
/// Signature: _ => v
extern const lauf_runtime_builtin lauf_lib_debug_read;

LAUF_HEADER_END

#endif // LAUF_LIB_DEBUG_H_INCLUDED

