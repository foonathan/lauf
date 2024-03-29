// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_BACKEND_DUMP_H_INCLUDED
#define LAUF_BACKEND_DUMP_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_asm_module              lauf_asm_module;
typedef struct lauf_asm_chunk               lauf_asm_chunk;
typedef struct lauf_writer                  lauf_writer;
typedef struct lauf_runtime_builtin         lauf_runtime_builtin_function;
typedef struct lauf_runtime_builtin_library lauf_runtime_builtin_library;

/// Options for the dump backend.
typedef struct lauf_backend_dump_options
{
    const lauf_runtime_builtin_library* builtin_libs;
    size_t                              builtin_libs_count;
} lauf_backend_dump_options;

/// The default dump options.
extern const lauf_backend_dump_options lauf_backend_default_dump_options;

/// Dumps the module (functions and globals, but not chunks) in a human readable format.
///
/// The format is not documented and subject to change.
/// It's only there to visually inspect and verify a module.
void lauf_backend_dump(lauf_writer* writer, lauf_backend_dump_options options,
                       const lauf_asm_module* mod);

/// Dumps the chunk of the module in a human readable format.
void lauf_backend_dump_chunk(lauf_writer* writer, lauf_backend_dump_options options,
                             const lauf_asm_module* mod, const lauf_asm_chunk* chunk);

LAUF_HEADER_END

#endif // LAUF_BACKEND_DUMP_H_INCLUDED

