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

//=== program ===//
/// A program that can be executed.
///
/// It consists of one more modules and an entry function.
/// All referenced external definitions must be resolved before execution.
/// This is done by matching the names.
typedef struct lauf_asm_program
{
    const lauf_asm_module*               _mod;
    const lauf_asm_function*             _entry;
    const struct lauf_asm_native_global* _global_defs;
} lauf_asm_program;

/// Creates a program that consists of a single module only.
lauf_asm_program lauf_asm_create_program(const lauf_asm_module*   mod,
                                         const lauf_asm_function* entry);

/// Creates a program that executes the given chunk.
lauf_asm_program lauf_asm_create_program_from_chunk(const lauf_asm_module* mod,
                                                    const lauf_asm_chunk*  chunk);

void lauf_asm_destroy_program(lauf_asm_program program);

//=== global definition ===//
typedef struct lauf_asm_native_global
{
    const struct lauf_asm_native_global* _next;
    const lauf_asm_global*               _global;
    void*                                _ptr;
    size_t                               _size;
} lauf_asm_native_global;

/// Defines a global that previously just declared to the native memory [ptr, ptr + size).
///
/// It will populate `lauf_asm_global_definition` to store information about the global.
/// Both `result` and `ptr` must live as long as the program and any process executing it.
void lauf_asm_define_native_global(lauf_asm_native_global* result, lauf_asm_program* program,
                                   const lauf_asm_global* global, void* ptr, size_t size);

//=== queries ===//
const char* lauf_asm_program_debug_path(const lauf_asm_program*  program,
                                        const lauf_asm_function* fn);

lauf_asm_debug_location lauf_asm_program_find_debug_location_of_instruction(
    const lauf_asm_program* program, const lauf_asm_inst* ip);

LAUF_HEADER_END

#endif // LAUF_ASM_PROGRAM_H_INCLUDED

