// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_TYPE_H_INCLUDED
#define LAUF_TYPE_H_INCLUDED

#include <lauf/config.h>
#include <lauf/value.h>

LAUF_HEADER_START

typedef struct lauf_layout
{
    size_t size, alignment;
} lauf_layout;

typedef struct lauf_type_data
{
    lauf_layout layout;
    size_t      field_count;
    lauf_value (*load_field)(const void* object_address, size_t field_index);
    void (*store_field)(void* object_address, size_t field_index, lauf_value value);
} lauf_type_data;

typedef const lauf_type_data* lauf_type;

LAUF_HEADER_END

#endif // LAUF_TYPE_H_INCLUDED

