// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_LIB_MEMORY_H_INCLUDED
#define LAUF_LIB_MEMORY_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_runtime_builtin         lauf_runtime_builtin;
typedef struct lauf_runtime_builtin_library lauf_runtime_builtin_library;

extern const lauf_runtime_builtin_library lauf_lib_memory;

/// Poisons the allocation a pointer is in.
///
/// It may not be accessed until un-poisoned again.
/// Poisoned memory may not be freed.
/// Poisoning a local variable may not have any effect if its pointer arithmetic has been optimized
/// out.
///
/// Signature: ptr:address => _
extern const lauf_runtime_builtin lauf_lib_memory_poison;

/// Unpoisons an allocation that was previously poisoned.
///
/// Signature: ptr:address => _
extern const lauf_runtime_builtin lauf_lib_memory_unpoison;

/// Splits an allocation into two parts.
///
/// Let `ptr` be an address for allocation `a` at offset `o`, which must not be the end.
/// Calling split shrinks the allocation `a` to size `o`,
/// and creates a new allocation `a'` for the region beginning at `o` until the original end of `a`.
/// It returns a pointer `ptr1` to `a` at offset `0` and a pointer `ptr2` to the beginning of `a'`.
///
/// Existing pointers to `a` are only valid if their offset is `< o`; otherwise, they become
/// invalidated. Neither `a` nor `a'` can be freed until the allocation has been merged again.
///
/// Signature: ptr:address => ptr1:address ptr2:address
extern const lauf_runtime_builtin lauf_lib_memory_split;

/// Merges an allocation that has been previously split.
///
/// It requires that `ptr1` and `ptr2` come from a call to `lauf_lib_memory_split` (ignoring the
/// offsets). It returns a pointer to the beginning of the merged allocation, which is the same as
/// `ptr1`.
///
/// All pointers to the allocation of `ptr2` are invalidated, pointers to allocation of `ptr1`
/// remain valid, and any pointers created before the split become valid again.
///
/// Signature: ptr1:address ptr2:address => ptr:address
extern const lauf_runtime_builtin lauf_lib_memory_merge;

LAUF_HEADER_END

#endif // LAUF_LIB_MEMORY_H_INCLUDED

