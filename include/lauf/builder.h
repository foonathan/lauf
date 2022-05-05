// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_BUILDER_H_INCLUDED
#define LAUF_BUILDER_H_INCLUDED

#include <lauf/config.h>
#include <lauf/function.h>
#include <lauf/value.h>

LAUF_HEADER_START

typedef struct lauf_BuilderImpl* lauf_Builder;

lauf_Builder lauf_builder(void);

void lauf_builder_destroy(lauf_Builder b);

void lauf_builder_function(lauf_Builder b, const char* name, lauf_FunctionSignature sig);

lauf_Function lauf_builder_end_function(lauf_Builder b);

//=== statements ===//
typedef enum lauf_Condition
{
    LAUF_IF_ZERO,
    LAUF_IF_NONZERO,
} lauf_Condition;

typedef struct lauf_BuilderIf
{
    size_t _jump_if, _jump_end;
} lauf_BuilderIf;

void lauf_builder_if(lauf_Builder b, lauf_BuilderIf* if_, lauf_Condition condition);

void lauf_builder_else(lauf_Builder b, lauf_BuilderIf* if_);

void lauf_builder_end_if(lauf_Builder b, lauf_BuilderIf* if_);

void lauf_builder_return(lauf_Builder b);

//=== expressions ===//
/// Pushes the specified constant integer onto the stack.
void lauf_builder_int(lauf_Builder b, lauf_ValueInt value);

/// Pushes the argument with the specified index onto the stack.
void lauf_builder_argument(lauf_Builder b, size_t idx);

/// Pops the top N values from the stack.
void lauf_builder_pop(lauf_Builder b, size_t n);

/// Calls the given builtin function.
/// It pops arguments from the stack and pushes its output.
void lauf_builder_call_builtin(lauf_Builder b, lauf_BuiltinFunction fn);

LAUF_HEADER_END

#endif // LAUF_BUILDER_H_INCLUDED

