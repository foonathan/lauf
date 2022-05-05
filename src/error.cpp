// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/error.h>

#include <cstdio>

namespace
{
void index_error(lauf_ErrorContext context, size_t size, size_t index)
{
    std::fprintf(stderr, "[lauf] %s:%s: index %zu out of range for collection of size %zu\n",
                 context.function, context.instruction, index, size);
}

void stack_overflow(lauf_ErrorContext context, size_t stack_size)
{
    std::fprintf(stderr, "[lauf] %s:%s: stack overflow of stack with size %zu\n", context.function,
                 context.instruction, stack_size);
}

void stack_underflow(lauf_ErrorContext context, size_t stack_size, size_t pop_count)
{
    std::fprintf(
        stderr,
        "[lauf] %s:%s: stack underflow while attempting to pop %zu value(s) from stack with size %zu\n",
        context.function, context.instruction, pop_count, stack_size);
}

void stack_nonempty(lauf_ErrorContext context, size_t stack_size)
{
    std::fprintf(stderr, "[lauf] %s:%s: %zu trailing values on stack\n", context.function,
                 context.instruction, stack_size);
}

void encoding_error(lauf_ErrorContext context, unsigned max_bits, size_t value)
{
    std::fprintf(stderr, "[lauf] %s:%s: encoding error of value %zu, maximal %u bits\n",
                 context.function, context.instruction, value, max_bits);
}
} // namespace

const lauf_ErrorHandler lauf_default_error_handler
    = {false, index_error, stack_overflow, stack_underflow, stack_nonempty, encoding_error};

