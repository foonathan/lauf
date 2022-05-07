// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_MODULE_H_INCLUDED
#define LAUF_MODULE_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

/// The signature of a function.
typedef struct lauf_signature
{
    /// The number of values the function pops from the stack as parameters.
    uint8_t input_count;
    /// The number of values the function pushes on the stack as return values.
    uint8_t output_count;
} lauf_signature;

//=== function ===//
/// A function that has been compiled to bytecode.
/// It is owned by a module.
typedef struct lauf_function_impl* lauf_function;

/// The name of the function for debugging purposes.
const char* lauf_function_get_name(lauf_function fn);

/// The signature of the function.
lauf_signature lauf_function_get_signature(lauf_function fn);

//=== module ===//
/// A module contains multiple functions.
typedef struct lauf_module_impl* lauf_module;

/// Destroys a module and all functions owned by it.
void lauf_module_destroy(lauf_module mod);

/// The name of the module for debugging purposes.
const char* lauf_module_get_name(lauf_module mod);

LAUF_HEADER_END

#endif // LAUF_MODULE_H_INCLUDED

