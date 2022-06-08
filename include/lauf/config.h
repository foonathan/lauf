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
#        define LAUF_HAS_TAIL_CALL 0
#    endif
#endif

#ifndef LAUF_HAS_COMPUTED_GOTO
#    if defined(__GNUC__)
#        define LAUF_HAS_COMPUTED_GOTO 1
#    else
#        define LAUF_HAS_COMPUTED_GOTO 0
#    endif
#endif

#ifndef LAUF_INLINE
#    if defined(__GNUC__)
#        define LAUF_INLINE __attribute__((always_inline)) inline
#    else
#        define LAUF_INLINE inline
#    endif
#endif

#ifndef LAUF_NOINLINE
#    if defined(__GNUC__)
#        define LAUF_NOINLINE __attribute__((noinline))
#    else
#        define LAUF_NOINLINE
#    endif
#endif

#if LAUF_HAS_TAIL_CALL
#    define LAUF_NOINLINE_IF_TAIL LAUF_NOINLINE
#else
#    define LAUF_NOINLINE_IF_TAIL
#endif

#ifndef LAUF_SECTION
#    if defined(__GNUC__)
#        define LAUF_SECTION(Name) __attribute__((section(Name), aligned(16)))
#    else
#        define LAUF_SECTION(Name)
#    endif
#endif

#endif // LAUF_CONFIG_H_INCLUDED

