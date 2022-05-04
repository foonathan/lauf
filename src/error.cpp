// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/error.h>

#include <cstdio>

namespace
{
void stack_underflow(lauf_ErrorContext context, size_t stack_size, size_t pop_count)
{
    std::fprintf(
        stderr,
        "[lauf] %s:%s: stack underflow while attempting to pop %zu value(s) from stack with size %zu\n",
        context.function, context.instruction, pop_count, stack_size);
}

void encoding_error(lauf_ErrorContext context, unsigned max_bits, size_t value)
{
    std::fprintf(stderr, "[lauf] %s:%s: encoding error of value %zu, maximal %u bits\n",
                 context.function, context.instruction, value, max_bits);
}
} // namespace

const lauf_ErrorHandler lauf_default_error_handler = {false, stack_underflow, encoding_error};

