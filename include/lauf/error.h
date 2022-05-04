// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_ERROR_H_INCLUDED
#define LAUF_ERROR_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>

typedef struct lauf_ErrorContext
{
    const char* function;
    const char* instruction;
} lauf_ErrorContext;

typedef struct lauf_ErrorHandler
{
    bool errors;
    void (*stack_underflow)(lauf_ErrorContext context, size_t stack_size, size_t pop_count);
    void (*encoding_error)(lauf_ErrorContext context, unsigned max_bits, size_t value);
} lauf_ErrorHandler;

extern const lauf_ErrorHandler lauf_default_error_handler;

#endif // LAUF_ERROR_H_INCLUDED

