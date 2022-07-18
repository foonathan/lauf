// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_ASM_PROGRAM_HPP_INCLUDED
#define SRC_LAUF_ASM_PROGRAM_HPP_INCLUDED

#include <lauf/asm/program.h>

struct lauf_asm_program
{
    lauf_asm_module*   mod;
    lauf_asm_function* entry;
};

#endif // SRC_LAUF_ASM_PROGRAM_HPP_INCLUDED

