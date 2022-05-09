// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_VALUE_H_INCLUDED
#define LAUF_VALUE_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef int64_t  lauf_value_int;
typedef uint64_t lauf_value_uint;
typedef void*    lauf_value_ptr;

typedef union lauf_value
{
    lauf_value_int  as_int;
    lauf_value_uint as_uint;
    lauf_value_ptr  as_ptr;
} lauf_value;

LAUF_HEADER_END

#endif // LAUF_VALUE_H_INCLUDED

