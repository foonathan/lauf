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

void lauf_builder_start_function(lauf_Builder b, const char* name, lauf_FunctionSignature sig);

lauf_Function lauf_builder_finish_function(lauf_Builder b);

//=== if ===//
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

//=== instructions ===//
/// Pushes the specified constant integer onto the stack.
void lauf_builder_push_int(lauf_Builder b, lauf_ValueInt value);

/// Pops the top N values from the stack.
void lauf_builder_pop(lauf_Builder b, size_t n);

/// Calls the given builtin function.
void lauf_builder_call_builtin(lauf_Builder b, lauf_BuiltinFunction fn);

LAUF_HEADER_END

#endif // LAUF_BUILDER_H_INCLUDED

