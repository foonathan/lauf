// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_CONFIG_H_INCLUDED
#define LAUF_CONFIG_H_INCLUDED

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if CHAR_BIT != 8
#    error "lauf requires 8 bit bytes"
#endif

#ifdef __cplusplus
#    define LAUF_HEADER_START                                                                      \
        extern "C"                                                                                 \
        {
#    define LAUF_HEADER_END }
#else
#    define LAUF_HEADER_START
#    define LAUF_HEADER_END
#endif

//=== attributes ===//
#ifndef LAUF_TAIL_CALL
#    if defined(__clang__)
#        define LAUF_TAIL_CALL __attribute__((musttail))
#        define LAUF_HAS_TAIL_CALL 1
#    else
#        define LAUF_TAIL_CALL
#        define LLAUF_HAS_TAIL_CALL 0
#    endif
#endif

#endif // LAUF_CONFIG_H_INCLUDED

