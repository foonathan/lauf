// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_RUNTIME_STACKTRACE_H_INCLUDED
#define LAUF_RUNTIME_STACKTRACE_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_asm_function    lauf_asm_function;
typedef union lauf_asm_inst         lauf_asm_inst;
typedef struct lauf_runtime_fiber   lauf_runtime_fiber;
typedef struct lauf_runtime_process lauf_runtime_process;

/// A stacktrace that is built when a panic occurs.
///
/// It initially points to some instruction of some function,
/// and can then be traversed until the top-level function is reached.
/// Each parent points to the call of the child.
typedef struct lauf_runtime_stacktrace lauf_runtime_stacktrace;

/// Returns the stacktrace of the fiber.
/// This returns nullptr if the fiber is neither running nor suspended.
///
/// If it returns a non-null value, it must either be iterated until the parent is found,
/// or manually destroyed.
lauf_runtime_stacktrace* lauf_runtime_get_stacktrace(lauf_runtime_process*     p,
                                                     const lauf_runtime_fiber* fiber);

/// Returns the function of the current stacktrace entry.
const lauf_asm_function* lauf_runtime_stacktrace_function(lauf_runtime_stacktrace* st);

/// Returns the address of the instruction of the current stacktrace entry.
const lauf_asm_inst* lauf_runtime_stacktrace_instruction(lauf_runtime_stacktrace* st);

/// Returns the parent call of the current stacktrace entry, or NULL if no parent exists.
/// If it returns NULL, the stacktrace is also destroyed.
lauf_runtime_stacktrace* lauf_runtime_stacktrace_parent(lauf_runtime_stacktrace* st);

/// Destroys a stacktrace.
void lauf_runtime_destroy_stacktrace(lauf_runtime_stacktrace* st);

LAUF_HEADER_END

#endif // LAUF_RUNTIME_STACKTRACE_H_INCLUDED

