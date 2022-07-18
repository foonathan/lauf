// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_RUNTIME_PROCESS_H_INCLUDED
#define LAUF_RUNTIME_PROCESS_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_runtime_stacktrace lauf_runtime_stacktrace;

/// Represents a currently running lauf program.
typedef struct lauf_runtime_process lauf_runtime_process;

/// Returns the current stacktrace of the process.
lauf_runtime_stacktrace* lauf_runtime_get_stacktrace(lauf_runtime_process* p);

LAUF_HEADER_END

#endif // LAUF_RUNTIME_PROCESS_H_INCLUDED

