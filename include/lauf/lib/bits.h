// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_LIB_BITS_H_INCLUDED
#define LAUF_LIB_BITS_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_runtime_builtin         lauf_runtime_builtin;
typedef struct lauf_runtime_builtin_library lauf_runtime_builtin_library;

extern const lauf_runtime_builtin_library lauf_lib_bits;

extern const lauf_runtime_builtin lauf_lib_bits_and;
extern const lauf_runtime_builtin lauf_lib_bits_or;
extern const lauf_runtime_builtin lauf_lib_bits_xor;

LAUF_HEADER_END

#endif // LAUF_LIB_BITS_H_INCLUDED

