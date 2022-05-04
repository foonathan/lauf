// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/function.h>

#include "detail/function.hpp"

lauf_Function lauf::create_function(const char* name, lauf_FunctionSignature sig,
                                    size_t max_stack_size, const lauf_Value* constants,
                                    size_t constant_count, const std::uint32_t* bytecode,
                                    size_t bytecode_size)
{
    auto memory_needed = sizeof(lauf_FunctionImpl) + sizeof(lauf_Value) * constant_count
                         + sizeof(std::uint32_t) * bytecode_size;
    auto memory = ::operator new(memory_needed);

    auto result = ::new (memory)
        lauf_FunctionImpl{name, sig, max_stack_size, constant_count, bytecode_size};
    std::memcpy(const_cast<lauf_Value*>(result->constant_begin()), constants,
                constant_count * sizeof(lauf_Value));
    std::memcpy(const_cast<std::uint32_t*>(result->bytecode_begin()), bytecode,
                bytecode_size * sizeof(std::uint32_t));
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
    return fn->signature;
}

