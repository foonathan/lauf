// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_CONFIG_H_INCLUDED
#define LAUF_CONFIG_H_INCLUDED

//=== basic definitions ===//
#ifdef __cplusplus

#    include <climits>
#    include <cstddef>
#    include <cstdint>

#    define LAUF_HEADER_START                                                                      \
        extern "C"                                                                                 \
        {
#    define LAUF_HEADER_END }

#else

#    include <limits.h>
#    include <stdbool.h>
#    include <stddef.h>
#    include <stdint.h>

#    define LAUF_HEADER_START
#    define LAUF_HEADER_END

#endif

//=== compatibility checks ===//
#if CHAR_BIT != 8
#    error "lauf assumes 8 bit bytes"
#endif

#if !defined(__GNUC__)
#    error "lauf currently requires GCC or GCC compatible compilers"
#endif

//=== optimizations ===//
#define LAUF_UNLIKELY(Cond) __builtin_expect((Cond), 0)
#define LAUF_TAIL_CALL [[clang::musttail]]
#define LAUF_NOINLINE [[gnu::noinline]]

//=== basic types ===//
typedef int64_t  lauf_sint;
typedef uint64_t lauf_uint;

#endif // LAUF_CONFIG_H_INCLUDED

