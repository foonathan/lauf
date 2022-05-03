// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_BUILDER_H_INCLUDED
#define LAUF_BUILDER_H_INCLUDED

#include <lauf/function.h>
#include <lauf/value.h>

typedef struct lauf_BuilderImpl* lauf_Builder;

lauf_Builder lauf_builder(void);

void lauf_builder_destroy(lauf_Builder b);

void lauf_builder_start_function(lauf_Builder b, const char* name, lauf_FunctionSignature sig);

lauf_Function lauf_builder_finish_function(lauf_Builder b);

//=== instructions ===//
/// A reference to a stack value.
/// It is invalidated when the value is popped.
typedef struct lauf_StackIndexImpl
{
    size_t impl;
} lauf_StackIndex;

/// Pushes the specified constant integer onto the stack.
lauf_StackIndex lauf_builder_push_int(lauf_Builder b, lauf_ValueInt value);

/// Pops the specified stack value and everything on top of it.
void lauf_builder_pop(lauf_Builder b, lauf_StackIndex idx);

#endif // LAUF_BUILDER_H_INCLUDED

