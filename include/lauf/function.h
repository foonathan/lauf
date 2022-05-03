// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_FUNCTION_H_INCLUDED
#define LAUF_FUNCTION_H_INCLUDED

#include <stddef.h>

typedef struct lauf_FunctionImpl* lauf_Function;

void lauf_function_destroy(lauf_Function fn);

const char* lauf_function_name(lauf_Function fn);

typedef struct lauf_FunctionSignature
{
    // The number of values the function pops from the stack as parameters.
    size_t input_count;
    // The number of values the function pushes on the stack as return values.
    size_t output_count;
} lauf_FunctionSignature;

lauf_FunctionSignature lauf_function_signature(lauf_Function fn);

#endif // LAUF_FUNCTION_H_INCLUDED

