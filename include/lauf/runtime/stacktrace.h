// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_RUNTIME_STACKTRACE_H_INCLUDED
#define LAUF_RUNTIME_STACKTRACE_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_asm_function lauf_asm_function;

/// A stacktrace that is built when a panic occurs.
///
/// It initially points to some instruction of some function,
/// and can then be traversed until the top-level function is reached.
/// Each parent points to the call of the child.
typedef struct lauf_runtime_stacktrace lauf_runtime_stacktrace;

/// Returns the function of the current stacktrace entry.
lauf_asm_function* lauf_runtime_stacktrace_function(lauf_runtime_stacktrace* bt);

/// Returns the address of the instruction of the current stacktrace entry.
const void* lauf_runtime_stacktrace_address(lauf_runtime_stacktrace* bt);

/// Returns the parent call of the current stacktrace entry, or NULL if no parent exists.
lauf_runtime_stacktrace* lauf_runtime_stacktrace_parent(lauf_runtime_stacktrace* bt);

LAUF_HEADER_END

#endif // LAUF_RUNTIME_STACKTRACE_H_INCLUDED

