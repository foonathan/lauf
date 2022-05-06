// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include "impl/module.hpp"

#include <new>

//=== function ===//
lauf_function lauf_impl_allocate_function(size_t bytecode_size)
{
    auto memory = ::operator new(sizeof(lauf_function_impl) + bytecode_size * sizeof(uint32_t));
    return static_cast<lauf_function>(memory);
}

const char* lauf_function_get_name(lauf_function fn)
{
    return fn->name;
}

lauf_signature lauf_function_get_signature(lauf_function fn)
{
    return {fn->input_count, fn->output_count};
}

//=== module ===//
lauf_module lauf_impl_allocate_module(size_t constant_count)
{
    auto memory = ::operator new(sizeof(lauf_module_impl) + constant_count * sizeof(lauf_value));
    return static_cast<lauf_module>(memory);
}

void lauf_module_destroy(lauf_module mod)
{
    auto fn = mod->first_function;
    while (fn != nullptr)
    {
        auto next = fn->next_function;
        ::   operator delete(fn);
        fn = next;
    }

    ::operator delete(mod);
}

const char* lauf_module_get_name(lauf_module mod)
{
    return mod->name;
}

lauf_function lauf_module_function_iter(lauf_module mod, lauf_function cur)
{
    if (cur == nullptr)
        return mod->first_function;
    else
        return cur->next_function;
}

