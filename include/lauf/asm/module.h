// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_ASM_MODULE_H_INCLUDED
#define LAUF_ASM_MODULE_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

//=== types ===//
/// A module, which is a self-contained unit of lauf ASM.
///
/// It consists of function definitions, declarations of externally provided functions, and global
/// memory. Each module corresponds to one (physical/virtual) source file.
typedef struct lauf_asm_module lauf_asm_module;

/// Global memory of a module.
typedef struct lauf_asm_global lauf_asm_global;

/// A function declaration.
///
/// It may or may not have a body associated with it.
typedef struct lauf_asm_function lauf_asm_function;

/// The signature of a function.
typedef struct lauf_asm_signature
{
    uint8_t input_count;
    uint8_t output_count;
} lauf_asm_signature;

//=== module ===//
/// Creates an empty module giving its name.
lauf_asm_module* lauf_asm_create_module(const char* name);

/// Destroys a module, and everything owned by it.
void lauf_asm_destroy_module(lauf_asm_module* mod);

/// Searches for a function by name.
///
/// This is not optimized.
lauf_asm_function* lauf_asm_find_function_by_name(lauf_asm_module* mod, const char* name);

//=== global memory ===//
/// Adds zero-initialized, mutable global memory of the specified size to the module.
lauf_asm_global* lauf_asm_add_global_zero_data(lauf_asm_module* mod, size_t size_in_bytes);

/// Adds the specified data as constant global memory to the module.
lauf_asm_global* lauf_asm_add_global_const_data(lauf_asm_module* mod, const void* data,
                                                size_t size_in_bytes);

/// Adds the specified data as mutable global memory to the module.
lauf_asm_global* lauf_asm_add_global_mut_data(lauf_asm_module* mod, const void* data,
                                              size_t size_in_bytes);

//=== functions ===//
/// Adds the declaration of a function with the specified name and signature.
lauf_asm_function* lauf_asm_add_function(lauf_asm_module* mod, const char* name,
                                         lauf_asm_signature sig);

LAUF_HEADER_END

#endif // LAUF_ASM_MODULE_H_INCLUDED

