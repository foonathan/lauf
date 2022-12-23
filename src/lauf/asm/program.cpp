// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/asm/program.h>

#include <lauf/asm/module.hpp>
#include <lauf/asm/program.hpp>

lauf_asm_program lauf_asm_create_program(const lauf_asm_module* mod, const lauf_asm_function* entry)
{
    return {mod, entry, nullptr};
}

lauf_asm_program lauf_asm_create_program_from_chunk(const lauf_asm_module* mod,
                                                    const lauf_asm_chunk*  chunk)
{
    return lauf_asm_create_program(mod, chunk->fn);
}

void lauf_asm_destroy_program(lauf_asm_program program)
{
    if (auto extra = lauf::try_get_extra_data(program))
        lauf::program_extra_data::destroy(extra);
}

void lauf_asm_define_native_global(lauf_asm_program* program, const lauf_asm_global* global,
                                   void* ptr, size_t size)
{
    auto& extra = lauf::get_extra_data(program);
    extra.add_definition(lauf::native_global_definition{global, ptr, size});
}

void lauf_asm_define_native_function(lauf_asm_program* program, const lauf_asm_function* fn,
                                     lauf_asm_native_function native_fn, void* user_data)
{
    auto& extra = lauf::get_extra_data(program);
    extra.add_definition(lauf::native_function_definition{fn, native_fn, user_data});
}

const char* lauf_asm_program_debug_path(const lauf_asm_program* program, const lauf_asm_function*)
{
    return program->_mod->debug_path;
}

lauf_asm_debug_location lauf_asm_program_find_debug_location_of_instruction(
    const lauf_asm_program* program, const lauf_asm_inst* ip)
{
    return lauf_asm_find_debug_location_of_instruction(program->_mod, ip);
}

