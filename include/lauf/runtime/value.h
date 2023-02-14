// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_RUNTIME_VALUE_H_INCLUDED
#define LAUF_RUNTIME_VALUE_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_runtime_address
{
    // Order of the fields chosen carefully:
    // acess to allocation is an AND, acess to offset a SHIFT, access to generation SHIFT + AND
    // (which is the one only necessary for checks). In addition, treating it as an integer and e.g.
    // incrementing it changes allocation first, not offset. That way, bugs are caught earlier.
    uint64_t allocation : 30;
    uint64_t generation : 2;
    uint64_t offset : 32;
} lauf_runtime_address;

static const lauf_runtime_address lauf_runtime_address_null = {0x3FFFFFFF, 0x3, 0xFFFFFFFF};

typedef struct lauf_runtime_function_address
{
    uint16_t index;
    uint8_t  input_count;
    uint8_t  output_count;
} lauf_runtime_function_address;

static const lauf_runtime_function_address lauf_runtime_function_address_null
    = {0xFFFF, 0xFF, 0xFF};

typedef union lauf_runtime_value
{
    lauf_uint                     as_uint;
    lauf_sint                     as_sint;
    void*                         as_native_ptr;
    lauf_runtime_address          as_address;
    lauf_runtime_function_address as_function_address;
} lauf_runtime_value;

LAUF_HEADER_END

#endif // LAUF_RUNTIME_VALUE_H_INCLUDED

