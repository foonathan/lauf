// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_ASM_PROGRAM_HPP_INCLUDED
#define SRC_LAUF_ASM_PROGRAM_HPP_INCLUDED

#include <lauf/asm/program.h>
#include <lauf/support/arena.hpp>
#include <lauf/support/array.hpp>

struct lauf_asm_program : lauf::intrinsic_arena<lauf_asm_program>
{
    const lauf_asm_module* mod = nullptr;

    lauf::array<const lauf_asm_function*> functions;
    const lauf_asm_function*              entry = nullptr;

    explicit lauf_asm_program(lauf::arena_key key) : lauf::intrinsic_arena<lauf_asm_program>(key) {}
};

#endif // SRC_LAUF_ASM_PROGRAM_HPP_INCLUDED

