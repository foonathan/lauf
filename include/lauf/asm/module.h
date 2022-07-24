// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_ASM_MODULE_H_INCLUDED
#define LAUF_ASM_MODULE_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_asm_layout lauf_asm_layout;

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

/// An instruction within a function.
///
/// The exact type is not exposed.
typedef union lauf_asm_inst lauf_asm_inst;

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
const lauf_asm_function* lauf_asm_find_function_by_name(const lauf_asm_module* mod,
                                                        const char*            name);

//=== global memory ===//
/// Adds zero-initialized, mutable global memory of the specified size to the module.
lauf_asm_global* lauf_asm_add_global_zero_data(lauf_asm_module* mod, lauf_asm_layout layout);

/// Adds the specified data as constant global memory to the module.
lauf_asm_global* lauf_asm_add_global_const_data(lauf_asm_module* mod, const void* data,
                                                lauf_asm_layout layout);

/// Adds the specified data as mutable global memory to the module.
lauf_asm_global* lauf_asm_add_global_mut_data(lauf_asm_module* mod, const void* data,
                                              lauf_asm_layout layout);

//=== functions ===//
/// Adds the declaration of a function with the specified name and signature.
lauf_asm_function* lauf_asm_add_function(lauf_asm_module* mod, const char* name,
                                         lauf_asm_signature sig);

const char*        lauf_asm_function_name(const lauf_asm_function* fn);
lauf_asm_signature lauf_asm_function_signature(const lauf_asm_function* fn);

/// Returns the index corresponding to the address of an instruction.
///
/// This can be used to translate e.g. the result of `lauf_runtime_stacktrace_address()` into a
/// persistent value.
size_t lauf_asm_get_instruction_index(const lauf_asm_function* fn, const lauf_asm_inst* ip);

LAUF_HEADER_END

#endif // LAUF_ASM_MODULE_H_INCLUDED

