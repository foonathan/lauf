// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_DETAIL_FUNCTION_HPP_INCLUDED
#define SRC_DETAIL_FUNCTION_HPP_INCLUDED

#include <cstddef>
#include <cstring>
#include <lauf/function.h>
#include <lauf/value.h>
#include <new>

struct alignas(std::max_align_t) lauf_FunctionImpl
{
    const char*            name;
    lauf_FunctionSignature signature;
    size_t                 max_stack_size;
    size_t                 constant_count, bytecode_size;

    const lauf_Value* constant_begin() const
    {
        auto memory = static_cast<const void*>(this + 1);
        return static_cast<const lauf_Value*>(memory);
    }
    const lauf_Value* constant_end() const
    {
        return constant_begin() + constant_count;
    }

    const unsigned char* bytecode_begin() const
    {
        auto memory = static_cast<const void*>(constant_end());
        return static_cast<const unsigned char*>(memory);
    }
    const unsigned char* bytecode_end() const
    {
        return bytecode_begin() + bytecode_size;
    }
};

namespace lauf
{
lauf_Function create_function(const char* name, lauf_FunctionSignature sig, size_t max_stack_size,
                              const lauf_Value* constants, size_t constant_count,
                              const unsigned char* bytecode, size_t bytecode_size);
} // namespace lauf

#endif // SRC_DETAIL_FUNCTION_HPP_INCLUDED

