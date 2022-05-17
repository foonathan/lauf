// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/impl/module.hpp>

#include <cassert>
#include <new>

//=== function ===//
lauf_function lauf_impl_allocate_function(size_t bytecode_size)
{
    auto memory = ::operator new(sizeof(lauf_function_impl) + bytecode_size * sizeof(uint32_t));
    return ::new (memory) lauf_function_impl{};
}

lauf_module lauf_function_get_module(lauf_function fn)
{
    return fn->mod;
}

lauf_signature lauf_function_get_signature(lauf_function fn)
{
    return {fn->input_count, fn->output_count};
}

/// The location of an instruction of the function with the specific address for debugging purposes.
lauf_debug_location lauf_function_get_location_of(lauf_function fn, void* inst);

const char* lauf_function_get_name(lauf_function fn)
{
    return fn->name;
}

lauf_debug_location lauf_function_get_location(lauf_function fn)
{
    if (fn->debug_locations == nullptr)
        return {};

    assert(fn->debug_locations[0].first_address < 0);
    return fn->debug_locations[0].location;
}

lauf_debug_location lauf_function_get_location_of(lauf_function fn, void* inst)
{
    if (fn->debug_locations == nullptr)
        return {};

    // This can be optimized to a binary search at the cost of storing the extra size.
    auto dest_offset = static_cast<lauf_vm_instruction*>(inst) - fn->bytecode();
    auto cur         = fn->debug_locations + 1;
    auto loc         = cur->location;
    while (cur->first_address >= 0)
    {
        if (cur->first_address > dest_offset)
            break;
        loc = cur->location;
        ++cur;
    }

    return loc;
}

//=== module ===//
lauf_module lauf_impl_allocate_module(size_t function_count, size_t literal_count)
{
    auto memory = ::operator new(sizeof(lauf_module_impl) + function_count * sizeof(lauf_function)
                                 + literal_count * sizeof(lauf_value));
    return ::new (memory) lauf_module_impl{};
}

void lauf_module_destroy(lauf_module mod)
{
    for (auto fn = mod->function_begin(); fn != mod->function_end(); ++fn)
    {
        delete[](*fn)->debug_locations;
        ::operator delete(*fn);
    }
    ::operator delete(mod);
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

