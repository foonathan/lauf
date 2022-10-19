// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/asm/program.h>

#include <lauf/asm/module.hpp>

lauf_asm_program lauf_asm_create_program(const lauf_asm_module* mod, const lauf_asm_function* entry)
{
    return {mod, entry};
}

lauf_asm_program lauf_asm_create_program_from_chunk(const lauf_asm_module* mod,
                                                    const lauf_asm_chunk*  chunk)
{
    return lauf_asm_create_program(mod, chunk->fn);
}

void lauf_asm_destroy_program(lauf_asm_program) {}

const char* lauf_asm_program_debug_path(const lauf_asm_program* program, const lauf_asm_function*)
{
    return program->_mod->debug_path;
}

lauf_asm_debug_location lauf_asm_program_find_debug_location_of_instruction(
    const lauf_asm_program* program, const lauf_asm_inst* ip)
{
    return lauf_asm_find_debug_location_of_instruction(program->_mod, ip);
}

