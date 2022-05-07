// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_IMPL_MODULE_HPP_INCLUDED
#define SRC_IMPL_MODULE_HPP_INCLUDED

#include "../detail/bytecode.hpp"
#include <lauf/module.h>
#include <lauf/value.h>

//=== function ===//
struct lauf_function_impl
{
    const char* name;
    uint32_t    _padding;
    uint16_t    max_vstack_size;
    uint8_t     input_count;
    uint8_t     output_count;

    lauf::_detail::bc_instruction* bytecode()
    {
        auto memory = static_cast<void*>(this + 1);
        return static_cast<lauf::_detail::bc_instruction*>(memory);
    }
};
static_assert(sizeof(lauf_function_impl) == sizeof(void*) + sizeof(uint64_t));
static_assert(sizeof(lauf_function_impl) % alignof(uint32_t) == 0);

lauf_function lauf_impl_allocate_function(size_t bytecode_size);

//=== module ===//
struct alignas(lauf_value) lauf_module_impl
{
    const char* name;
    size_t      function_count;

    lauf_function* function_begin()
    {
        auto memory = static_cast<void*>(this + 1);
        return static_cast<lauf_function*>(memory);
    }
    lauf_function* function_end()
    {
        return function_begin() + function_count;
    }
    lauf_function get_function(lauf::_detail::bc_function_idx idx)
    {
        return function_begin()[static_cast<size_t>(idx)];
    }

    lauf_value* constant_data()
    {
        auto memory = static_cast<void*>(function_end());
        return static_cast<lauf_value*>(memory);
    }
    const lauf_value& get_constant(lauf::_detail::bc_constant_idx idx)
    {
        return constant_data()[static_cast<size_t>(idx)];
    }
};
static_assert(sizeof(lauf_module_impl) == 2 * sizeof(void*));
static_assert(sizeof(lauf_module_impl) % alignof(lauf_value) == 0);

lauf_module lauf_impl_allocate_module(size_t function_count, size_t constant_count);

#endif // SRC_IMPL_MODULE_HPP_INCLUDED

