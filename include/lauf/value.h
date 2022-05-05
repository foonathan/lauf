// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_VALUE_H_INCLUDED
#define LAUF_VALUE_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef int64_t lauf_ValueInt;
typedef void*   lauf_ValuePtr;

typedef union lauf_Value
{
    lauf_ValueInt as_int;
    lauf_ValuePtr as_ptr;
} lauf_Value;

LAUF_HEADER_END

#endif // LAUF_VALUE_H_INCLUDED

