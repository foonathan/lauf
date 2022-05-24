// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_VALUE_H_INCLUDED
#define LAUF_VALUE_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

//=== integers ===//
typedef int64_t       lauf_value_sint;
const lauf_value_sint lauf_value_sint_min = INT64_MIN;
const lauf_value_sint lauf_value_sint_max = INT64_MAX;

typedef uint64_t      lauf_value_uint;
const lauf_value_sint lauf_value_uint_max = UINT64_MAX;

//=== address ===//
typedef const void* lauf_value_native_ptr;

typedef struct lauf_value_address
{
    uint32_t allocation;
    uint32_t offset;
} lauf_value_address;

//=== value ===//
typedef union lauf_value
{
    lauf_value_sint       as_sint;
    lauf_value_uint       as_uint;
    lauf_value_address    as_address;
    lauf_value_native_ptr as_native_ptr; // SAFETY: only access from literal values
} lauf_value;

LAUF_HEADER_END

#endif // LAUF_VALUE_H_INCLUDED

