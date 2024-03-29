// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_BACKEND_WRITER_H_INCLUDED
#define LAUF_BACKEND_WRITER_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

/// Interface for specifying where the output of a backend should be written to.
typedef struct lauf_writer lauf_writer;

/// Destroys a writer, releasing all resources.
void lauf_destroy_writer(lauf_writer* writer);

//=== string writer ===//
/// Returns a writer that writes to a C string.
lauf_writer* lauf_create_string_writer(void);

/// Returns the string after everything has been written.
const char* lauf_writer_get_string(lauf_writer* string_writer);

//=== file writer ===//
/// Returns a writer that writes to the specified path.
///
/// The file is only completely written once `lauf_backend_destroy_writer()` has been called.
lauf_writer* lauf_create_file_writer(const char* path);

//=== stdout writer ===//
/// Returns a writer that writes to stdout.
lauf_writer* lauf_create_stdout_writer(void);

LAUF_HEADER_END

#endif // LAUF_BACKEND_WRITER_H_INCLUDED

