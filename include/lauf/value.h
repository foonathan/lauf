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
    // Order of the fields chosen carefully:
    // acess to allocation is an AND, acess to offset a shift, access to generation shift + and
    // (which is the one only necessary for checks). In addition, treating it as an integer and e.g.
    // incrementing it changes allocation first, not offset. That way, bugs are caught earlier.
    uint64_t allocation : 30;
    uint64_t generation : 2;
    uint64_t offset : 32;
} lauf_value_address;

const lauf_value_address lauf_value_address_invalid = {(uint32_t)(-1) >> 2, 0, 0};

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

