// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_BUILTIN_H_INCLUDED
#define LAUF_BUILTIN_H_INCLUDED

#include <lauf/config.h>
#include <lauf/vm.h>

LAUF_HEADER_START

typedef bool lauf_builtin_function(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr,
                                   lauf_vm vm);

bool lauf_builtin_dispatch(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr,
                           lauf_vm vm);

typedef struct lauf_builtin
{
    lauf_signature         signature;
    lauf_builtin_function* impl;
} lauf_builtin;

LAUF_HEADER_END

#endif // LAUF_BUILTIN_H_INCLUDED

