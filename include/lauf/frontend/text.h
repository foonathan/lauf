// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_FRONTEND_TEXT_H_INCLUDED
#define LAUF_FRONTEND_TEXT_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_reader                  lauf_reader;
typedef struct lauf_asm_module              lauf_asm_module;
typedef struct lauf_asm_type                lauf_asm_type;
typedef struct lauf_runtime_builtin         lauf_runtime_builtin_function;
typedef struct lauf_runtime_builtin_library lauf_runtime_builtin_library;

/// Options for the text frontend.
typedef struct lauf_frontend_text_options
{
    const lauf_runtime_builtin_library* builtin_libs;
    size_t                              builtin_libs_count;
} lauf_frontend_text_options;

/// The default text options.
extern const lauf_frontend_text_options lauf_frontend_default_text_options;

/// Reads a module in text format.
///
/// If the input is well-formed, returns the module.
/// Otherwise, logs errors to stderr and returns nullptr.
lauf_asm_module* lauf_frontend_text(lauf_reader* reader, lauf_frontend_text_options options);

LAUF_HEADER_END

#endif // LAUF_FRONTEND_TEXT_H_INCLUDED

