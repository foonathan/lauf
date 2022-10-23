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

typedef int64_t  lauf_sint;
typedef uint64_t lauf_uint;

//=== compatibility checks ===//
#if CHAR_BIT != 8
#    error "lauf assumes 8 bit bytes"
#endif

#if !defined(__clang__)
#    error "lauf currently requires clang"
#endif

//=== optimizations ===//
#define LAUF_LIKELY(Cond) __builtin_expect((Cond), 1)
#define LAUF_UNLIKELY(Cond) __builtin_expect((Cond), 0)
#define LAUF_TAIL_CALL [[clang::musttail]]
#define LAUF_NOINLINE [[gnu::noinline]]
#define LAUF_FORCE_INLINE [[gnu::always_inline]]
#define LAUF_UNREACHABLE __builtin_unreachable()

//=== configurations ===//
#ifndef LAUF_CONFIG_DISPATCH_JUMP_TABLE
#    define LAUF_CONFIG_DISPATCH_JUMP_TABLE 1
#endif

#endif // LAUF_CONFIG_H_INCLUDED

