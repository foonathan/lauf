// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_ASM_PROGRAM_H_INCLUDED
#define LAUF_ASM_PROGRAM_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_asm_module   lauf_asm_module;
typedef struct lauf_asm_function lauf_asm_function;

/// A program that can be executed.
///
/// It consists of one more modules and an entry function.
/// All referenced external definitions must be resolved before execution.
/// This is done by matching the names.
typedef struct lauf_asm_program lauf_asm_program;

/// Creates a program that consists of a single module only.
lauf_asm_program* lauf_asm_create_program(lauf_asm_module* mod, lauf_asm_function* fn);

void lauf_asm_destroy_program(lauf_asm_program* program);

lauf_asm_function* lauf_asm_entry_function(lauf_asm_program* program);

LAUF_HEADER_END

#endif // LAUF_ASM_PROGRAM_H_INCLUDED

