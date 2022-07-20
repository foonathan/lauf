// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/asm/program.hpp>

#include <lauf/asm/module.hpp>

lauf_asm_program* lauf_asm_create_program(const lauf_asm_module*   mod,
                                          const lauf_asm_function* entry)
{
    auto program = lauf_asm_program::create();

    program->mod = mod;
    program->functions.resize_uninitialized(*program, mod->functions_count);
    for (auto fn = mod->functions; fn != nullptr; fn = fn->next)
        program->functions[fn->function_idx] = fn;
    program->entry = entry->function_idx;

    return program;
}

void lauf_asm_destroy_program(lauf_asm_program* program)
{
    lauf_asm_program::destroy(program);
}

const lauf_asm_function* lauf_asm_entry_function(const lauf_asm_program* program)
{
    return program->functions[program->entry];
}

