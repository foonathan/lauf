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

/// A chunk of code.
///
/// It is like a function that takes no arguments, but it can be re-used and cleared.
/// It is meant for temporary code that isn't used a lot.
typedef struct lauf_asm_chunk lauf_asm_chunk;

/// An instruction within a function or chunk.
///
/// The exact type is not exposed.
typedef union lauf_asm_inst lauf_asm_inst;

/// The signature of a function.
typedef struct lauf_asm_signature
{
    uint8_t input_count;
    uint8_t output_count;
} lauf_asm_signature;

/// The debug location of an entity.
typedef struct lauf_asm_debug_location
{
    uint16_t line_nr;          /// 1 based, 0 means unknown
    uint16_t column_nr : 15;   /// 1 based, 0 means unknown
    bool     is_synthetic : 1; /// true if code was injected
} lauf_asm_debug_location;

//=== module ===//
/// Creates an empty module giving its name.
lauf_asm_module* lauf_asm_create_module(const char* name);

/// Destroys a module, and everything owned by it.
void lauf_asm_destroy_module(lauf_asm_module* mod);

/// Sets the path of the module.
/// This is only used for debug information.
void lauf_asm_set_module_debug_path(lauf_asm_module* mod, const char* path);

/// Returns the debug path of the module.
const char* lauf_asm_module_debug_path(lauf_asm_module* mod);

/// Searches for a function by name.
///
/// This is not optimized.
const lauf_asm_function* lauf_asm_find_function_by_name(const lauf_asm_module* mod,
                                                        const char*            name);

/// Retrieves the function or chunk that contains the instruction.
///
/// This is not optimized.
const lauf_asm_function* lauf_asm_find_function_of_instruction(const lauf_asm_module* mod,
                                                               const lauf_asm_inst*   ip);
const lauf_asm_chunk*    lauf_asm_find_chunk_of_instruction(const lauf_asm_module* mod,
                                                            const lauf_asm_inst*   ip);

/// Retrieves the associated debug location of an instruction.
///
/// This is not optimized.
lauf_asm_debug_location lauf_asm_find_debug_location_of_instruction(const lauf_asm_module* mod,
                                                                    const lauf_asm_inst*   ip);

//=== global memory ===//
typedef enum lauf_asm_global_permissions
{
    LAUF_ASM_GLOBAL_READ_ONLY,
    LAUF_ASM_GLOBAL_READ_WRITE,
} lauf_asm_global_permissions;

/// Adds a new global variable with the specified permissions.
/// It is only a declaration of a memory location that is not resolved yet.
/// It can be either resolved to native memory when creating the program, or by specifying data.
lauf_asm_global* lauf_asm_add_global(lauf_asm_module* mod, lauf_asm_global_permissions perms);

/// Defines an undeclared global variable to contain the specified memory (initially).
/// If `data == nullptr`, it is zero-initialized.
void lauf_asm_define_data_global(lauf_asm_module* mod, lauf_asm_global* global,
                                 lauf_asm_layout layout, const void* data);

/// Set a name of a global variable.
/// This is only used for debugging purposes.
void lauf_asm_set_global_debug_name(lauf_asm_module* mod, lauf_asm_global* global,
                                    const char* name);

/// Whether or not the global is defined.
bool lauf_asm_global_has_definition(const lauf_asm_global* global);

/// Returns the layout of a defined global.
lauf_asm_layout lauf_asm_global_layout(const lauf_asm_global* global);

/// Returns the debug name, or nullptr if none was given.
const char* lauf_asm_global_debug_name(const lauf_asm_global* global);

//=== functions ===//
/// Adds the declaration of a function with the specified name and signature.
lauf_asm_function* lauf_asm_add_function(lauf_asm_module* mod, const char* name,
                                         lauf_asm_signature sig);

/// Exports a function.
/// This is only relevant for backends that generate assembly.
void lauf_asm_export_function(lauf_asm_function* fn);

const char*        lauf_asm_function_name(const lauf_asm_function* fn);
lauf_asm_signature lauf_asm_function_signature(const lauf_asm_function* fn);
bool               lauf_asm_function_has_definition(const lauf_asm_function* fn);

/// Returns the index corresponding to the address of an instruction.
///
/// This can be used to translate e.g. the result of `lauf_runtime_stacktrace_address()` into a
/// persistent value.
size_t lauf_asm_get_instruction_index(const lauf_asm_function* fn, const lauf_asm_inst* ip);

//=== chunks ===//
/// Creates a new chunk for the module.
///
/// Each chunk uses a separate arena for memory allocation, so they should be re-used when possible.
lauf_asm_chunk* lauf_asm_create_chunk(lauf_asm_module* mod);

lauf_asm_signature lauf_asm_chunk_signature(const lauf_asm_chunk* chunk);
bool               lauf_asm_chunk_is_empty(const lauf_asm_chunk* chunk);

LAUF_HEADER_END

#endif // LAUF_ASM_MODULE_H_INCLUDED

