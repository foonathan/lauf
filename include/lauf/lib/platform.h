// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_LIB_PLATFORM_H_INCLUDED
#define LAUF_LIB_PLATFORM_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_runtime_builtin         lauf_runtime_builtin;
typedef struct lauf_runtime_builtin_library lauf_runtime_builtin_library;

extern const lauf_runtime_builtin_library lauf_lib_platform;

/// Returns true if currently running on the VM, false otherwise.
/// Signature: _ => bool
extern const lauf_runtime_builtin lauf_lib_platform_vm;

/// Returns true if currently running on the QBE backend, false otherwise.
/// Signature: _ => bool
extern const lauf_runtime_builtin lauf_lib_platform_qbe;

LAUF_HEADER_END

#endif // LAUF_LIB_PLATFORM_H_INCLUDED

