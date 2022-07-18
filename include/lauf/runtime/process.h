// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_RUNTIME_PROCESS_H_INCLUDED
#define LAUF_RUNTIME_PROCESS_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_runtime_stacktrace lauf_runtime_stacktrace;
typedef struct lauf_runtime_address    lauf_runtime_address;
typedef struct lauf_asm_layout         lauf_asm_layout;

//=== process ===//
/// Represents a currently running lauf program.
typedef struct lauf_runtime_process lauf_runtime_process;

/// Returns the current stacktrace of the process.
lauf_runtime_stacktrace* lauf_runtime_get_stacktrace(lauf_runtime_process* p);

//=== address ===//
/// Converts an address to a pointer if the address is readable for the layout.
/// Returns null otherwise.
const void* lauf_runtime_get_const_ptr(lauf_runtime_process* p, lauf_runtime_address addr,
                                       lauf_asm_layout layout);

/// Converts an address to a pointer if the address is readable and writeable for the layout.
/// Returns null otherwise.
void* lauf_runtime_get_mut_ptr(lauf_runtime_process* p, lauf_runtime_address addr,
                               lauf_asm_layout layout);

/// Converts an address to a pointer if there is a null-terminated string starting at address.
/// Returns null otherwise.
const char* lauf_runtime_get_cstr(lauf_runtime_process* p, lauf_runtime_address addr);

LAUF_HEADER_END

#endif // LAUF_RUNTIME_PROCESS_H_INCLUDED

