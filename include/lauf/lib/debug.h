// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_LIB_DEBUG_H_INCLUDED
#define LAUF_LIB_DEBUG_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_runtime_builtin_function lauf_runtime_builtin_function;

extern const lauf_runtime_builtin_function* lauf_lib_debug;

/// Prints the top value of the stack for debugging purposes.
///
/// Signature: v => v
extern const lauf_runtime_builtin_function lauf_lib_debug_print;

/// Prints the entire vstack for debugging purposes.
///
/// Signature: _ => _
extern const lauf_runtime_builtin_function lauf_lib_debug_print_vstack;

/// Prints a stacktrace.
///
/// Signature: _ => _
extern const lauf_runtime_builtin_function lauf_lib_debug_print_cstack;

/// Breaks into the debugger.
///
/// Signature: _ => _
extern const lauf_runtime_builtin_function lauf_lib_debug_break;

LAUF_HEADER_END

#endif // LAUF_LIB_DEBUG_H_INCLUDED

