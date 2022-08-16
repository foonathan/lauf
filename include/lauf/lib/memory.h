// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_LIB_MEMORY_H_INCLUDED
#define LAUF_LIB_MEMORY_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_runtime_builtin         lauf_runtime_builtin;
typedef struct lauf_runtime_builtin_library lauf_runtime_builtin_library;

extern const lauf_runtime_builtin_library lauf_lib_memory;

/// Poisons an allocation.
///
/// It may not be accessed until un-poisoned again.
/// Poisoned memory may not be freed.
/// Poisoning a local variable may not have any effect if its pointer arithmetic has been optimized
/// out.
///
/// Signature: ptr:address => _
extern const lauf_runtime_builtin lauf_lib_memory_poison;

/// Unpoisons an allocation that was previously poisned.
///
/// Signature: ptr:address => _
extern const lauf_runtime_builtin lauf_lib_memory_unpoison;

LAUF_HEADER_END

#endif // LAUF_LIB_MEMORY_H_INCLUDED

