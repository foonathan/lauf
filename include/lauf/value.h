// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_VALUE_H_INCLUDED
#define LAUF_VALUE_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef int64_t     lauf_value_sint;
typedef uint64_t    lauf_value_uint;
typedef const void* lauf_value_ptr;

const lauf_value_sint lauf_value_sint_min = INT64_MIN;
const lauf_value_sint lauf_value_sint_max = INT64_MAX;

const lauf_value_sint lauf_value_uint_max = UINT64_MAX;

typedef union lauf_value
{
    lauf_value_sint as_sint;
    lauf_value_uint as_uint;
    lauf_value_ptr  as_ptr;
} lauf_value;

LAUF_HEADER_END

#endif // LAUF_VALUE_H_INCLUDED

