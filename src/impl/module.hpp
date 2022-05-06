// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_IMPL_MODULE_HPP_INCLUDED
#define SRC_IMPL_MODULE_HPP_INCLUDED

#include <lauf/module.h>
#include <lauf/value.h>

//=== function ===//
struct lauf_function_impl
{
    lauf_function next_function;
    const char*   name;
    uint32_t      _padding;
    uint16_t      max_vstack_size;
    uint8_t       input_count;
    uint8_t       output_count;

    const uint32_t* bytecode() const
    {
        auto memory = static_cast<const void*>(this + 1);
        return static_cast<const uint32_t*>(memory);
    }
};
static_assert(sizeof(lauf_function_impl) == 2 * sizeof(void*) + 8);
static_assert(sizeof(lauf_function_impl) % alignof(uint32_t) == 0);

lauf_function lauf_impl_allocate_function(size_t bytecode_size);

//=== module ===//
struct alignas(lauf_value) lauf_module_impl
{
    const char*   name;
    lauf_function first_function;

    lauf_value* constant_data()
    {
        auto memory = static_cast<void*>(this + 1);
        return static_cast<lauf_value*>(memory);
    }
    const lauf_value& get_constant(size_t idx) const
    {
        auto memory = static_cast<const void*>(this + 1);
        return static_cast<const lauf_value*>(memory)[idx];
    }
};
static_assert(sizeof(lauf_module_impl) == 2 * sizeof(void*));
static_assert(sizeof(lauf_module_impl) % alignof(lauf_value) == 0);

lauf_module lauf_impl_allocate_module(size_t constant_count);

#endif // SRC_IMPL_MODULE_HPP_INCLUDED

