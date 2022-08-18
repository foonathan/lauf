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
/// It may not be accessed until un-poisoned again, but can be freed.
/// Poisoning a local variable may not have any effect if its pointer arithmetic has been optimized
/// out.
///
/// Signature: addr:address => _
extern const lauf_runtime_builtin lauf_lib_memory_poison;

/// Unpoisons an allocation that was previously poisoned.
///
/// Signature: addr:address => _
extern const lauf_runtime_builtin lauf_lib_memory_unpoison;

/// Splits an allocation into two parts.
///
/// Let `addr` be an address for allocation `a` at offset `o`, which must not be the end.
/// Calling split shrinks the allocation `a` to size `o`,
/// and creates a new allocation `a'` for the region beginning at `o` until the original end of `a`.
/// It returns a pointer `addr1` to `a` at offset `0` and a pointer `addr2` to the beginning of
/// `a'`.
///
/// Existing pointers to `a` are only valid if their offset is `< o`; otherwise, they become
/// invalidated. Neither `a` nor `a'` can be freed until the allocation has been merged again.
///
/// Signature: addr:address => addr1:address addr2:address
extern const lauf_runtime_builtin lauf_lib_memory_split;

/// Merges an allocation that has been previously split.
///
/// It requires that `addr1` and `addr2` come from a call to `lauf_lib_memory_split` (ignoring the
/// offsets). It returns a pointer to the beginning of the merged allocation, which is the same as
/// `addr1`.
///
/// All pointers to the allocation of `addr2` are invalidated, pointers to allocation of `addr1`
/// remain valid, and any pointers created before the split become valid again.
///
/// Signature: addr1:address addr2:address => addr:address
extern const lauf_runtime_builtin lauf_lib_memory_merge;

/// Converts an address to an integer.
///
/// In addition to the integer it returns provenance information that must be kept available to
/// convert the integer back to the address. This ensures that the integer cannot be manipulated to
/// point to a different allocation.
///
/// Signature: addr:address => provenance:address uint
extern const lauf_runtime_builtin lauf_lib_memory_addr_to_int;

/// Converts an integer with provenance back to an address.
///
/// Signature: provenance:address uint => addr:address
extern const lauf_runtime_builtin lauf_lib_memory_int_to_addr;

LAUF_HEADER_END

#endif // LAUF_LIB_MEMORY_H_INCLUDED

