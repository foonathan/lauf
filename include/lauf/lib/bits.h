// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_LIB_BITS_H_INCLUDED
#define LAUF_LIB_BITS_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_runtime_builtin         lauf_runtime_builtin;
typedef struct lauf_runtime_builtin_library lauf_runtime_builtin_library;

extern const lauf_runtime_builtin_library lauf_lib_bits;

//=== primitive bit operations ===//
/// Signature: a:uint b:uint => (a op b):uint
extern const lauf_runtime_builtin lauf_lib_bits_and;
extern const lauf_runtime_builtin lauf_lib_bits_or;
extern const lauf_runtime_builtin lauf_lib_bits_xor;

/// Shifts x by n to the left.
///
/// Panics if n >= bits(uint). Discards bits shifted out.
///
/// Signature: x:uint n:uint => (x << n):uint
extern const lauf_runtime_builtin lauf_lib_bits_shl;

/// Shifts x by n to the right.
///
/// Panics if n >= bits(uint). Discards bits shifted out. Fills with zeroes on the left.
///
/// Signature: x:uint n:uint => (x >> n):uint
extern const lauf_runtime_builtin lauf_lib_bits_ushr;

/// Shifts x by n to the right.
///
/// Panics if n >= bits(sint). Discards bits shifted out. Fills with sign bit on the left.
///
/// Signature: x:sint n:uint => (x >> n):sint
extern const lauf_runtime_builtin lauf_lib_bits_sshr;

LAUF_HEADER_END

#endif // LAUF_LIB_BITS_H_INCLUDED

