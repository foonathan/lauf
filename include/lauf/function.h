// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_FUNCTION_H_INCLUDED
#define LAUF_FUNCTION_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_FunctionImpl* lauf_Function;

typedef struct lauf_FunctionSignature
{
    // The number of values the function pops from the stack as parameters.
    size_t input_count;
    // The number of values the function pushes on the stack as return values.
    size_t output_count;
} lauf_FunctionSignature;

union lauf_Value;
typedef ptrdiff_t lauf_BuiltinFunction(union lauf_Value* stack_ptr);

lauf_Function lauf_builtin_function(const char* name, lauf_FunctionSignature sig,
                                    lauf_BuiltinFunction* fn);

void lauf_function_destroy(lauf_Function fn);

const char* lauf_function_name(lauf_Function fn);

lauf_FunctionSignature lauf_function_signature(lauf_Function fn);

bool lauf_function_is_builtin(lauf_Function fn);

LAUF_HEADER_END

#endif // LAUF_FUNCTION_H_INCLUDED

