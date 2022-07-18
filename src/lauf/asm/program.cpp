// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/asm/program.hpp>

lauf_asm_program* lauf_asm_create_program(const lauf_asm_module* mod, const lauf_asm_function* fn)
{
    return new lauf_asm_program{mod, fn};
}

void lauf_asm_destroy_program(lauf_asm_program* program)
{
    delete program;
}

const lauf_asm_function* lauf_asm_entry_function(const lauf_asm_program* program)
{
    return program->entry;
}

