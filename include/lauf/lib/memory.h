// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_LIB_MEMORY_H_INCLUDED
#define LAUF_LIB_MEMORY_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_runtime_builtin         lauf_runtime_builtin;
typedef struct lauf_runtime_builtin_library lauf_runtime_builtin_library;
typedef struct lauf_runtime_process         lauf_runtime_process;

extern const lauf_runtime_builtin_library lauf_lib_memory;

//=== allocation flagging ===//
/// Calls `lauf_runtime_poison_allocation()`.
///
/// Signature: addr:address => _
extern const lauf_runtime_builtin lauf_lib_memory_poison;

/// Calls `lauf_runtime_unpoison_allocation()`.
///
/// Signature: addr:address => _
extern const lauf_runtime_builtin lauf_lib_memory_unpoison;

/// Calls `lauf_runtime_split_allocation()`.
///
/// Signature: addr:address => addr1:address addr2:address
extern const lauf_runtime_builtin lauf_lib_memory_split;

/// Calls `lauf_runtime_merge_allocation()`.
///
/// It returns an address for the beginning of the merged allocation, which is the same as
/// `addr1`.
///
/// Signature: addr1:address addr2:address => addr:address
extern const lauf_runtime_builtin lauf_lib_memory_merge;

//=== address manipulation ===//
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

/// Adds an offset to an address.
///
/// This may invalidate the address.
///
/// Signature: addr:address offset:sint => (addr + offset):address
extern const lauf_runtime_builtin lauf_lib_memory_addr_add;
/// Subtracts an offset from an address.
///
/// This may invalidate the address.
///
/// Signature: addr:address offset:sint => (addr - offset):address
extern const lauf_runtime_builtin lauf_lib_memory_addr_sub;

/// Returns the distance in bytes between two addresses.
///
/// The addresses must be in the same allocation.
///
/// Signature: addr1:address addr2:address => (addr1 - addr2):sint
extern const lauf_runtime_builtin lauf_lib_memory_addr_distance;

LAUF_HEADER_END

#endif // LAUF_LIB_MEMORY_H_INCLUDED

