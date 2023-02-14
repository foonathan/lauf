// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_BACKEND_QBE_H_INCLUDED
#define LAUF_BACKEND_QBE_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_asm_module              lauf_asm_module;
typedef struct lauf_writer                  lauf_writer;
typedef struct lauf_runtime_builtin         lauf_runtime_builtin_function;
typedef struct lauf_runtime_builtin_library lauf_runtime_builtin_library;

/// Generates a function call in order to implement this builtin.
///
/// Each input value maps to a pointer-sized argument in order from left to right.
/// The return value depends on the output count:
/// * For 0, it returns void.
/// * For 1, it returns a single pointer-sized value.
/// * Otherwise, it returns a struct containing N pointer-size objects, filled from lower to higher
/// addresses.
typedef struct lauf_backend_qbe_extern_function
{
    const char*                 name;
    const lauf_runtime_builtin* builtin;
} lauf_backend_qbe_extern_function;

/// Options for the QBE backend.
typedef struct lauf_backend_qbe_options
{
    const lauf_backend_qbe_extern_function* extern_fns;
    size_t                                  extern_fns_count;
} lauf_backend_qbe_options;

/// The default QBE options.
extern const lauf_backend_qbe_options lauf_backend_default_qbe_options;

/// Converts a module into the intermediate language of [QBE](https://c9x.me/compile/),
/// which can then be converted into native assembly code.
void lauf_backend_qbe(lauf_writer* writer, lauf_backend_qbe_options options,
                      const lauf_asm_module* mod);

LAUF_HEADER_END

#endif // LAUF_BACKEND_QBE_H_INCLUDED

