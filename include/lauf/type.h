// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_TYPE_H_INCLUDED
#define LAUF_TYPE_H_INCLUDED

#include <lauf/config.h>
#include <lauf/value.h>

LAUF_HEADER_START

//=== layout ===//
typedef struct lauf_layout
{
    size_t size, alignment;
} lauf_layout;

#define LAUF_NATIVE_LAYOUT_OF(Type)                                                                \
    lauf_layout                                                                                    \
    {                                                                                              \
        sizeof(Type), alignof(Type)                                                                \
    }

lauf_layout lauf_array_layout(lauf_layout base, size_t length);

//=== type ===//
typedef struct lauf_type_data
{
    lauf_layout layout;
    size_t      field_count;
    lauf_value (*load_field)(const void* object_address, size_t field_index);
    bool (*store_field)(void* object_address, size_t field_index, lauf_value value);
} lauf_type_data;

typedef const lauf_type_data* lauf_type;

extern const lauf_type_data lauf_value_type;

/// Generates lauf_type_data for `NativeType` that consists of a single value.
#define LAUF_NATIVE_SINGLE_VALUE_TYPE(Name, NativeType, ValueField)                                \
    static lauf_value Name##_load_field(const void* object_address, size_t)                        \
    {                                                                                              \
        lauf_value result;                                                                         \
        result.ValueField = *(const NativeType*)(object_address);                                  \
        return result;                                                                             \
    }                                                                                              \
    static bool Name##_store_field(void* object_address, size_t, lauf_value value)                 \
    {                                                                                              \
        *(NativeType*)(object_address) = value.ValueField;                                         \
        return true;                                                                               \
    }                                                                                              \
    const lauf_type_data Name                                                                      \
        = {LAUF_NATIVE_LAYOUT_OF(NativeType), 1, &Name##_load_field, &Name##_store_field}

LAUF_HEADER_END

#endif // LAUF_TYPE_H_INCLUDED

