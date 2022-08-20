// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_ASM_PROGRAM_H_INCLUDED
#define LAUF_ASM_PROGRAM_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_asm_module         lauf_asm_module;
typedef struct lauf_asm_function       lauf_asm_function;
typedef struct lauf_asm_debug_location lauf_asm_location;
typedef union lauf_asm_inst            lauf_asm_inst;

/// A program that can be executed.
///
/// It consists of one more modules and an entry function.
/// All referenced external definitions must be resolved before execution.
/// This is done by matching the names.
typedef struct lauf_asm_program lauf_asm_program;

/// Creates a program that consists of a single module only.
lauf_asm_program* lauf_asm_create_program(const lauf_asm_module*   mod,
                                          const lauf_asm_function* entry);

void lauf_asm_destroy_program(lauf_asm_program* program);

const lauf_asm_function* lauf_asm_program_entry_function(const lauf_asm_program* program);

const char* lauf_asm_program_debug_path(const lauf_asm_program*  program,
                                        const lauf_asm_function* fn);

lauf_asm_debug_location lauf_asm_program_find_debug_location_of_instruction(
    const lauf_asm_program* program, const lauf_asm_inst* ip);

LAUF_HEADER_END

#endif // LAUF_ASM_PROGRAM_H_INCLUDED

