// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_RUNTIME_VALUE_H_INCLUDED
#define LAUF_RUNTIME_VALUE_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef union lauf_runtime_value
{
    lauf_uint as_uint;
    lauf_sint as_sint;
} lauf_runtime_value;

LAUF_HEADER_END

#endif // LAUF_RUNTIME_VALUE_H_INCLUDED

