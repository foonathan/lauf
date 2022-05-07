// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_BUILDER_H_INCLUDED
#define LAUF_BUILDER_H_INCLUDED

#include <lauf/config.h>
#include <lauf/module.h>
#include <lauf/type.h>
#include <lauf/value.h>

LAUF_HEADER_START

struct lauf_builtin;

typedef enum lauf_condition
{
    LAUF_IF_TRUE,
    LAUF_IF_FALSE,
} lauf_condition;

//=== module builder ===//
typedef struct lauf_module_builder_impl* lauf_module_builder;

lauf_module_builder lauf_build_module(const char* name);

lauf_module lauf_finish_module(lauf_module_builder b);

//=== function builder ===//
typedef struct lauf_function_builder_impl* lauf_function_builder;

typedef struct lauf_local_variable
{
    lauf_value_address _addr;
} lauf_local_variable;

lauf_function_builder lauf_build_function(lauf_module_builder b, const char* name,
                                          lauf_signature sig);

lauf_function lauf_finish_function(lauf_function_builder b);

lauf_local_variable lauf_build_local_variable(lauf_function_builder b, lauf_layout layout);

//=== block builder ===//
typedef struct lauf_block_builder_impl* lauf_block_builder;

lauf_block_builder lauf_build_block(lauf_function_builder b, lauf_signature sig);

void lauf_finish_block_return(lauf_block_builder b);
void lauf_finish_block_jump(lauf_block_builder b, lauf_block_builder dest);
void lauf_finish_block_branch(lauf_block_builder b, lauf_condition condition,
                              lauf_block_builder if_true, lauf_block_builder if_false);

//=== instructions ===//
void lauf_build_int(lauf_block_builder b, lauf_value_int value);
void lauf_build_argument(lauf_block_builder b, size_t idx);
void lauf_build_local_addr(lauf_block_builder b, lauf_local_variable var);

void lauf_build_drop(lauf_block_builder b, size_t n);
void lauf_build_pick(lauf_block_builder b, size_t n);
void lauf_build_roll(lauf_block_builder b, size_t n);

void lauf_build_recurse(lauf_block_builder b);
void lauf_build_call(lauf_block_builder b, lauf_function_builder fn);
void lauf_build_call_builtin(lauf_block_builder b, struct lauf_builtin fn);

void lauf_build_load_field(lauf_block_builder b, lauf_type type, size_t field);
void lauf_build_store_field(lauf_block_builder b, lauf_type type, size_t field);

LAUF_HEADER_END

#endif // LAUF_BUILDER_H_INCLUDED

