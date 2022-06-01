// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/impl/module.hpp>

#include <cassert>
#include <new>

//=== function ===//
lauf_module lauf_function_get_module(lauf_function fn)
{
    return fn->mod;
}

lauf_signature lauf_function_get_signature(lauf_function fn)
{
    return {fn->input_count, fn->output_count};
}

const char* lauf_function_get_name(lauf_function fn)
{
    return fn->name;
}

lauf_debug_location lauf_function_get_location_of(lauf_function fn, void* inst)
{
    auto offset = static_cast<lauf_vm_instruction*>(inst) - fn->bytecode();
    return fn->debug_locations.location_of(offset);
}

//=== module ===//
void lauf_module_destroy(lauf_module mod)
{
    lauf_module_impl::destroy(mod);
}

const char* lauf_module_get_name(lauf_module mod)
{
    return mod->name;
}
const char* lauf_module_get_path(lauf_module mod)
{
    return mod->path;
}

lauf_function* lauf_module_function_begin(lauf_module mod)
{
    return mod->function_begin();
}

lauf_function* lauf_module_function_end(lauf_module mod)
{
    return mod->function_end();
}

