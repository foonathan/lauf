// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/asm/module.hpp>

lauf_asm_module* lauf_asm_create_module(const char* name)
{
    return lauf_asm_module::create(name);
}

void lauf_asm_destroy_module(lauf_asm_module* mod)
{
    lauf_asm_module::destroy(mod);
}

lauf_asm_global* lauf_asm_add_global_zero_data(lauf_asm_module* mod, size_t size_in_bytes)
{
    return mod->construct<lauf_asm_global>(mod, size_in_bytes);
}

lauf_asm_global* lauf_asm_add_global_const_data(lauf_asm_module* mod, const void* data,
                                                size_t size_in_bytes)
{
    return mod->construct<lauf_asm_global>(mod, data, size_in_bytes, lauf_asm_global::read_only);
}

lauf_asm_global* lauf_asm_add_global_mut_data(lauf_asm_module* mod, const void* data,
                                              size_t size_in_bytes)
{
    return mod->construct<lauf_asm_global>(mod, data, size_in_bytes, lauf_asm_global::read_write);
}

lauf_asm_function* lauf_asm_add_function(lauf_asm_module* mod, const char* name,
                                         lauf_asm_signature sig)
{
    return mod->construct<lauf_asm_function>(mod, name, sig);
}
