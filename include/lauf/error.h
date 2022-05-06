// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_ERROR_H_INCLUDED
#define LAUF_ERROR_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_error_context
{
    const char* function;
    const char* instruction;
} lauf_error_context;

typedef struct lauf_error_handler
{
    bool errors;
    void (*index_error)(lauf_error_context context, size_t size, size_t index);
    void (*stack_overflow)(lauf_error_context context, size_t stack_size);
    void (*stack_underflow)(lauf_error_context context, size_t stack_size, size_t pop_count);
    void (*stack_nonempty)(lauf_error_context context, size_t stack_size);
    void (*encoding_error)(lauf_error_context context, unsigned max_bits, size_t value);
} lauf_error_handler;

extern const lauf_error_handler lauf_default_error_handler;

LAUF_HEADER_END

#endif // LAUF_ERROR_H_INCLUDED

