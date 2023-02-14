// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_READER_H_INCLUDED
#define LAUF_READER_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

/// Interface for specifying where the input of a frontend should be read from.
///
/// It can be used multiple times, representing the same input every time.
typedef struct lauf_reader lauf_reader;

void lauf_destroy_reader(lauf_reader* reader);

/// Overrides the path associated with a reader.
/// The path needs to remain valid until the reader is destroyed.
void lauf_reader_set_path(lauf_reader* reader, const char* path);

/// Returns a reader that will return the specified string.
lauf_reader* lauf_create_string_reader(const char* str, size_t size);
lauf_reader* lauf_create_cstring_reader(const char* str);

/// Returns a reader that reads from the specified path.
/// Returns null, if the file could not be opened or read.
/// The path needs to remain valid until the reader is destroyed.
lauf_reader* lauf_create_file_reader(const char* path);

/// Returns a writer that reads from stdin.
lauf_reader* lauf_create_stdin_reader(void);

LAUF_HEADER_END

#endif // LAUF_READER_H_INCLUDED

