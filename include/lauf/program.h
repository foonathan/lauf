// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_PROGRAM_H_INCLUDED
#define LAUF_PROGRAM_H_INCLUDED

#include <lauf/config.h>
#include <lauf/module.h>

LAUF_HEADER_START

typedef struct lauf_program_impl
{
    void* _data[2];
} lauf_program;

void lauf_program_destroy(lauf_program prog);

lauf_signature lauf_program_get_signature(lauf_program prog);

lauf_function lauf_program_get_entry_function(lauf_program prog);

LAUF_HEADER_END

#endif // LAUF_PROGRAM_H_INCLUDED

