// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_ASM_PROGRAM_H_INCLUDED
#define LAUF_ASM_PROGRAM_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_asm_module         lauf_asm_module;
typedef struct lauf_asm_global         lauf_asm_global;
typedef struct lauf_asm_function       lauf_asm_function;
typedef struct lauf_asm_chunk          lauf_asm_chunk;
typedef struct lauf_asm_debug_location lauf_asm_location;
typedef union lauf_asm_inst            lauf_asm_inst;
typedef struct lauf_runtime_process    lauf_runtime_process;
typedef union lauf_runtime_value       lauf_runtime_value;

//=== program ===//
/// A program that can be executed.
///
/// It consists of one more modules and an entry function.
/// All referenced external definitions must be resolved before execution.
/// This is done by matching the names.
typedef struct lauf_asm_program
{
    const lauf_asm_module*   _mod;
    const lauf_asm_function* _entry;
    void*                    _extra_data;
} lauf_asm_program;

/// Creates a program that consists of a single module only.
lauf_asm_program lauf_asm_create_program(const lauf_asm_module*   mod,
                                         const lauf_asm_function* entry);

/// Creates a program that executes the given chunk.
lauf_asm_program lauf_asm_create_program_from_chunk(const lauf_asm_module* mod,
                                                    const lauf_asm_chunk*  chunk);

void lauf_asm_destroy_program(lauf_asm_program program);

//=== native definition ===//
typedef bool (*lauf_asm_native_function)(void* user_data, lauf_runtime_process* process,
                                         const lauf_runtime_value* input,
                                         lauf_runtime_value*       output);

/// Defines a previously declared global as native memory [ptr, ptr + size).
void lauf_asm_define_native_global(lauf_asm_program* program, const lauf_asm_global* global,
                                   void* ptr, size_t size);

/// Defines a previously declared function as the specified native function.
///
/// The native function will be invoked with the input arguments in `input` (top of the stack at
/// high index). On success, it shall return true and write the output values to `output` (top of
/// the stack at high index). On error, it may panic by returning `lauf_runtime_panic()`.
/// `input` and `output` do not alias.
void lauf_asm_define_native_function(lauf_asm_program* program, const lauf_asm_function* fn,
                                     lauf_asm_native_function native_fn, void* user_data);

//=== queries ===//
const char* lauf_asm_program_debug_path(const lauf_asm_program*  program,
                                        const lauf_asm_function* fn);

lauf_asm_debug_location lauf_asm_program_find_debug_location_of_instruction(
    const lauf_asm_program* program, const lauf_asm_inst* ip);

LAUF_HEADER_END

#endif // LAUF_ASM_PROGRAM_H_INCLUDED

