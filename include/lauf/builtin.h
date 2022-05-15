// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_BUILTIN_H_INCLUDED
#define LAUF_BUILTIN_H_INCLUDED

#include <lauf/config.h>
#include <lauf/module.h>

LAUF_HEADER_START

bool lauf_builtin_dispatch(void* ip, union lauf_value* stack_ptr, void* frame_ptr, void* vm);

typedef bool lauf_builtin_function(void* ip, union lauf_value* stack_ptr, void* frame_ptr,
                                   void* vm);

typedef struct lauf_builtin
{
    lauf_signature         signature;
    lauf_builtin_function* impl;
} lauf_builtin;

LAUF_HEADER_END

#endif // LAUF_BUILTIN_H_INCLUDED

