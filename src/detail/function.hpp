// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_DETAIL_FUNCTION_HPP_INCLUDED
#define SRC_DETAIL_FUNCTION_HPP_INCLUDED

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <lauf/function.h>
#include <lauf/value.h>
#include <new>

struct alignas(std::max_align_t) lauf_FunctionImpl
{
    const char* name;
    uint32_t    bytecode_size;
    uint16_t    max_vstack_size;
    uint8_t     input_count;
    uint8_t     output_count;

    const std::uint32_t* bytecode_begin() const
    {
        auto memory = static_cast<const void*>(this + 1);
        return static_cast<const std::uint32_t*>(memory);
    }
    const std::uint32_t* bytecode_end() const
    {
        return bytecode_begin() + bytecode_size;
    }

    const lauf_Value* constants() const
    {
        auto memory = static_cast<const void*>(bytecode_end());
        return static_cast<const lauf_Value*>(memory);
    }
};

namespace lauf
{
lauf_Function create_function(const char* name, lauf_Signature sig, uint16_t max_vstack_size,
                              const lauf_Value* constants, size_t constant_count,
                              const std::uint32_t* bytecode, size_t bytecode_size);
} // namespace lauf

#endif // SRC_DETAIL_FUNCTION_HPP_INCLUDED

