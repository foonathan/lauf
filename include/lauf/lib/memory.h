// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_LIB_MEMORY_H_INCLUDED
#define LAUF_LIB_MEMORY_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_runtime_builtin         lauf_runtime_builtin;
typedef struct lauf_runtime_builtin_library lauf_runtime_builtin_library;

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
/// Controls overflow behavior of arithmetic on memory addresses.
/// Overflow occurrs when the offset within an allocation exceeds the allocation boundary.
typedef enum lauf_lib_memory_addr_overflow
{
    /// Overflow invalidates the address, but does not panic (yet).
    LAUF_LIB_MEMORY_ADDR_OVERFLOW_INVALIDATE,
    /// Overflow panics. It is allowed to form an offset to the end of the allocation.
    LAUF_LIB_MEMORY_ADDR_OVERFLOW_PANIC,
    /// Overflow panics. It is not allowed to form an offset to the end of the allocatio; address
    /// will remain valid.
    LAUF_LIB_MEMORY_ADDR_OVERFLOW_PANIC_STRICT,
} lauf_lib_memory_addr_overflow;

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
/// Signature: addr:address offset:sint => (addr + offset):address
lauf_runtime_builtin lauf_lib_memory_addr_add(lauf_lib_memory_addr_overflow overflow);

/// Subtracts an offset from an address.
///
/// Signature: addr:address offset:sint => (addr - offset):address
lauf_runtime_builtin lauf_lib_memory_addr_sub(lauf_lib_memory_addr_overflow overflow);

/// Returns the distance in bytes between two addresses.
///
/// The addresses must be in the same allocation.
///
/// Signature: addr1:address addr2:address => (addr1 - addr2):sint
extern const lauf_runtime_builtin lauf_lib_memory_addr_distance;

//=== memcpy/memset/memcmp ===//
/// Copies `count` bytes from `src` to `dest` by calling `std::memmove()`.
///
/// Signature: dest:address src:address count:uint => _
extern const lauf_runtime_builtin lauf_lib_memory_copy;

/// Sets `count` bytes from `dest` to `byte` by calling `std::memset()`.
///
/// Signature: dest:address byte:uint count:uint => _
extern const lauf_runtime_builtin lauf_lib_memory_fill;

/// Compares `count` bytes between `lhs` and `rhs` by calling `std::memcmp()`.
///
/// Signature: lhs:address rhs:address count:uint => sint
extern const lauf_runtime_builtin lauf_lib_memory_cmp;

LAUF_HEADER_END

#endif // LAUF_LIB_MEMORY_H_INCLUDED

