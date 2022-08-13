// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_LIB_TEST_H_INCLUDED
#define LAUF_LIB_TEST_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_runtime_builtin         lauf_runtime_builtin;
typedef struct lauf_runtime_builtin_library lauf_runtime_builtin_library;

/// A collection of functions especially designed for writing tests for lauf in lauf.
extern const lauf_runtime_builtin_library lauf_lib_test;

/// Asserts that something is not reachable.
/// Panics otherwise.
///
/// Signature: _ => _
extern const lauf_runtime_builtin lauf_lib_test_unreachable;

/// Asserts that the value on top of the stack is non-zero.
/// Prints it and panics otherwise.
///
/// Signature: top => _
extern const lauf_runtime_builtin lauf_lib_test_assert;

/// Asserts that the two values on top of the stack are bitwise equal.
/// Prints them and panics otherwise.
///
/// Signature: a b => _
extern const lauf_runtime_builtin lauf_lib_test_assert_eq;

/// Asserts that a function does panic with the specified message.
///
/// It expects a function address on the vstack with signature 0 => 0, calls it,
/// and panics if it does not panic with the specified message.
///
/// Signature: fn msg => _
extern const lauf_runtime_builtin lauf_lib_test_assert_panic;

LAUF_HEADER_END

#endif // LAUF_LIB_TEST_H_INCLUDED

