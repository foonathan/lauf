// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_IMPL_MODULE_HPP_INCLUDED
#define SRC_IMPL_MODULE_HPP_INCLUDED

#include <lauf/detail/bytecode.hpp>
#include <lauf/module.h>
#include <lauf/value.h>

//=== debug metadata ===//
namespace lauf::_detail
{
struct debug_location_map
{
    ptrdiff_t           first_address;
    lauf_debug_location location;
};
} // namespace lauf::_detail

//=== function ===//
struct lauf_function_impl
{
    lauf_module mod;
    const char* name;
    uint16_t    _padding;
    uint16_t    local_stack_size;
    uint16_t    max_vstack_size;
    uint8_t     input_count;
    uint8_t     output_count;
    // debug_locations[0] has first_address < 0 and stores location of function
    // debug_locations[N - 1] has first_address < 0 and is sentinel
    lauf::_detail::debug_location_map* debug_locations;

    lauf_vm_instruction* bytecode()
    {
        auto memory = static_cast<void*>(this + 1);
        return static_cast<lauf_vm_instruction*>(memory);
    }
};
static_assert(sizeof(lauf_function_impl) == 3 * sizeof(void*) + sizeof(uint64_t));
static_assert(sizeof(lauf_function_impl) % alignof(uint32_t) == 0);

lauf_function lauf_impl_allocate_function(size_t bytecode_size);

//=== module ===//
struct alignas(lauf_value) lauf_module_impl
{
    const char* name;
    const char* path;
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

    lauf_value* literal_data()
    {
        auto memory = static_cast<void*>(function_end());
        return static_cast<lauf_value*>(memory);
    }
};
static_assert(sizeof(lauf_module_impl) == 3 * sizeof(void*));
static_assert(sizeof(lauf_module_impl) % alignof(lauf_value) == 0);

lauf_module lauf_impl_allocate_module(size_t function_count, size_t literal_count);

#endif // SRC_IMPL_MODULE_HPP_INCLUDED

