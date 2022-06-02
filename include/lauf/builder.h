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
    LAUF_IS_TRUE,
    LAUF_IS_FALSE,

    LAUF_CMP_EQ,
    LAUF_CMP_NE,
    LAUF_CMP_LT,
    LAUF_CMP_LE,
    LAUF_CMP_GE,
    LAUF_CMP_GT,
} lauf_condition;

//=== builder ===//
typedef struct lauf_builder_impl* lauf_builder;

lauf_builder lauf_builder_create(void);
void         lauf_builder_destroy(lauf_builder b);

/// Sets the location for the following module/function/instructions.
void lauf_build_debug_location(lauf_builder b, lauf_debug_location location);

//=== module ===//
typedef struct lauf_function_decl
{
    size_t _idx;
} lauf_function_decl;

typedef struct lauf_global
{
    size_t _idx;
} lauf_global;

void        lauf_build_module(lauf_builder b, const char* name, const char* path);
lauf_module lauf_finish_module(lauf_builder b);

lauf_function_decl lauf_declare_function(lauf_builder b, const char* name,
                                         lauf_signature signature);

// Note: memory is not owned by resulting module.
lauf_global lauf_build_const(lauf_builder b, const void* memory, size_t size);
// Note: memory is not owned by resulting module, copied for each execution.
lauf_global lauf_build_data(lauf_builder b, const void* memory, size_t size);
lauf_global lauf_build_zero_data(lauf_builder b, size_t size);

//=== function ===//
typedef struct lauf_local
{
    size_t _addr;
    size_t _size;
} lauf_local;

typedef struct lauf_label
{
    size_t _idx;
} lauf_label;

void          lauf_build_function(lauf_builder b, lauf_function_decl fn);
lauf_function lauf_finish_function(lauf_builder b);

lauf_local lauf_build_local_variable(lauf_builder b, lauf_layout layout);

lauf_label lauf_declare_label(lauf_builder b, size_t vstack_size);
void       lauf_place_label(lauf_builder b, lauf_label label);

//=== instructions ===//
void lauf_build_return(lauf_builder b);
void lauf_build_jump(lauf_builder b, lauf_label dest);
void lauf_build_jump_if(lauf_builder b, lauf_condition condition, lauf_label dest);

void lauf_build_int(lauf_builder b, lauf_value_sint value);
void lauf_build_global_addr(lauf_builder b, lauf_global global);
void lauf_build_local_addr(lauf_builder b, lauf_local var);
void lauf_build_layout_of(lauf_builder b, lauf_type type);

void lauf_build_pop(lauf_builder b, size_t n);
void lauf_build_pick(lauf_builder b, size_t n);
void lauf_build_roll(lauf_builder b, size_t n);
void lauf_build_drop(lauf_builder b, size_t n);
void lauf_build_select(lauf_builder b, size_t max_index);
void lauf_build_select_if(lauf_builder b, lauf_condition condition);

void lauf_build_call(lauf_builder b, lauf_function_decl fn);
void lauf_build_call_builtin(lauf_builder b, struct lauf_builtin fn);

void lauf_build_array_element_addr(lauf_builder b, lauf_type type);
void lauf_build_load_field(lauf_builder b, lauf_type type, size_t field);
void lauf_build_store_field(lauf_builder b, lauf_type type, size_t field);

void lauf_build_load_value(lauf_builder b, lauf_local var);
void lauf_build_load_array_value(lauf_builder b, lauf_local var);
void lauf_build_store_value(lauf_builder b, lauf_local var);
void lauf_build_store_array_value(lauf_builder b, lauf_local var);

void lauf_build_panic(lauf_builder b);
void lauf_build_panic_if(lauf_builder b, lauf_condition condition);

LAUF_HEADER_END

#endif // LAUF_BUILDER_H_INCLUDED

