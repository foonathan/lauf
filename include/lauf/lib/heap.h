// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_LIB_HEAP_H_INCLUDED
#define LAUF_LIB_HEAP_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_runtime_builtin         lauf_runtime_builtin;
typedef struct lauf_runtime_builtin_library lauf_runtime_builtin_library;

extern const lauf_runtime_builtin_library lauf_lib_heap;

/// Allocates new heap memory using the allocator of the VM.
///
/// Signature: alignment:uint size:uint => ptr:address
extern const lauf_runtime_builtin lauf_lib_heap_alloc;

/// Frees previously allocated heap memory.
///
/// Signature: ptr:address => _
extern const lauf_runtime_builtin lauf_lib_heap_free;

/// Marks heap memory as freed without it being actually freed.
///
/// This prevents code from ever accessing it again.
///
/// Signature: ptr:address => _
extern const lauf_runtime_builtin lauf_lib_heap_leak;

LAUF_HEADER_END

#endif // LAUF_LIB_HEAP_H_INCLUDED

