// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_ASM_TYPE_H_INCLUDED
#define LAUF_ASM_TYPE_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

/// The layout of a type.
typedef struct lauf_asm_layout
{
    size_t size;
    size_t alignment;
} lauf_asm_layout;

LAUF_HEADER_END

#endif // LAUF_ASM_TYPE_H_INCLUDED

