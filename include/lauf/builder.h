// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_BUILDER_H_INCLUDED
#define LAUF_BUILDER_H_INCLUDED

#include <lauf/config.h>
#include <lauf/module.h>
#include <lauf/value.h>

LAUF_HEADER_START

struct lauf_builtin;

typedef struct lauf_builder_impl* lauf_builder;

lauf_builder lauf_build(const char* mod_name);

lauf_module lauf_build_finish(lauf_builder b);

//=== functions ===//
typedef struct lauf_builder_function
{
    size_t _fn;
} lauf_builder_function;

lauf_builder_function lauf_build_declare_function(lauf_builder b, const char* fn_name,
                                                  lauf_signature signature);
void                  lauf_build_start_function(lauf_builder b, lauf_builder_function fn);
lauf_function         lauf_build_finish_function(lauf_builder b);

//=== statements ===//
typedef enum lauf_condition
{
    LAUF_IF_ZERO,
    LAUF_IF_NONZERO,
} lauf_condition;

typedef struct lauf_builder_if
{
    size_t _jump_if, _jump_end;
} lauf_builder_if;

lauf_builder_if lauf_build_if(lauf_builder b, lauf_condition condition);
void            lauf_build_else(lauf_builder b, lauf_builder_if* if_);
void            lauf_build_end_if(lauf_builder b, lauf_builder_if* if_);

void lauf_build_return(lauf_builder b);

//=== expressions ===//
void lauf_build_int(lauf_builder b, lauf_value_int value);
void lauf_build_argument(lauf_builder b, size_t idx);

void lauf_build_pop(lauf_builder b, size_t n);

void lauf_build_call(lauf_builder b, lauf_function fn);
void lauf_build_call_decl(lauf_builder b, lauf_builder_function fn);
void lauf_build_call_builtin(lauf_builder b, struct lauf_builtin fn);

LAUF_HEADER_END

#endif // LAUF_BUILDER_H_INCLUDED

