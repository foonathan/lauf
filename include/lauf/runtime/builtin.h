// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_RUNTIME_BUILTIN_H_INCLUDED
#define LAUF_RUNTIME_BUILTIN_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef union lauf_runtime_value    lauf_runtime_value;
typedef struct lauf_runtime_process lauf_runtime_process;

typedef enum lauf_runtime_builtin_flags
{
    LAUF_RUNTIME_BUILTIN_DEFAULT = 0,

    /// The builtin will never panic.
    LAUF_RUNTIME_BUILTIN_NO_PANIC = 1 << 0,
    /// The builtin does not need the process.
    /// Logically mplies no panic.
    LAUF_RUNTIME_BUILTIN_NO_PROCESS = 1 << 1,

    /// The builtin can only be used with the VM and not in other backends.
    LAUF_RUNTIME_BUILTIN_VM_ONLY = 1 << 2,
} lauf_runtime_builtin_flags;

/// The signature of the implementation of a builtin.
///
/// `input` will point to the first argument in the vstack, which is the one that was pushed first:
/// `input[input_count - 1]` is the left most argument, `input[0]` the rightmost argument.
///
/// `output` will point to the location where the first output value should be pushed to:
/// `output[output_count - 1]` is the first return value, `output[0]` the last one which will be on
/// top of the stack.
///
/// `input` and `output` can (and do) alias, so you need to read all inputs before writing any
/// outputs.
typedef bool lauf_runtime_builtin_function_impl(lauf_runtime_process*     process,
                                                const lauf_runtime_value* input,
                                                lauf_runtime_value*       output);

typedef struct lauf_runtime_builtin_function
{
    lauf_runtime_builtin_function_impl*  impl;
    uint8_t                              input_count;
    uint8_t                              output_count;
    int                                  flags;
    const char*                          name;
    const lauf_runtime_builtin_function* next;
} lauf_runtime_builtin_function;

LAUF_HEADER_END

#endif // LAUF_RUNTIME_BUILTIN_H_INCLUDED

