// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/function.h>

#include "detail/function.hpp"

lauf_Function lauf::create_function(const char* name, lauf_FunctionSignature sig,
                                    uint16_t max_stack_size, const lauf_Value* constants,
                                    size_t constant_count, const std::uint32_t* bytecode,
                                    size_t bytecode_size)
{
    auto memory_needed = sizeof(lauf_FunctionImpl) + sizeof(std::uint32_t) * bytecode_size
                         + sizeof(lauf_Value) * constant_count;
    auto memory = ::operator new(memory_needed);

    auto result            = ::new (memory) lauf_FunctionImpl{};
    result->name           = name;
    result->input_count    = sig.input_count;
    result->output_count   = sig.output_count;
    result->max_stack_size = max_stack_size;

    result->bytecode_size = bytecode_size;
    std::memcpy(const_cast<std::uint32_t*>(result->bytecode_begin()), bytecode,
                bytecode_size * sizeof(std::uint32_t));

    std::memcpy(const_cast<lauf_Value*>(result->constants()), constants,
                constant_count * sizeof(lauf_Value));

    return result;
}

void lauf_function_destroy(lauf_Function fn)
{
    ::operator delete(static_cast<void*>(fn));
}

const char* lauf_function_name(lauf_Function fn)
{
    return fn->name;
}

lauf_FunctionSignature lauf_function_signature(lauf_Function fn)
{
    return {fn->input_count, fn->output_count};
}

