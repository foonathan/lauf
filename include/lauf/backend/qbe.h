// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_BACKEND_QBE_H_INCLUDED
#define LAUF_BACKEND_QBE_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_asm_module              lauf_asm_module;
typedef struct lauf_writer                  lauf_writer;
typedef struct lauf_runtime_builtin         lauf_runtime_builtin_function;
typedef struct lauf_runtime_builtin_library lauf_runtime_builtin_library;

/// Options for the QBE backend.
typedef struct lauf_backend_qbe_options
{
    int _dummy;
} lauf_backend_qbe_options;

/// The default QBE options.
extern const lauf_backend_qbe_options lauf_backend_default_qbe_options;

/// Converts a module into the intermediate language of [QBE](https://c9x.me/compile/),
/// which can then be converted into native assembly code.
void lauf_backend_qbe(lauf_writer* writer, lauf_backend_qbe_options options,
                      const lauf_asm_module* mod);

LAUF_HEADER_END

#endif // LAUF_BACKEND_QBE_H_INCLUDED

