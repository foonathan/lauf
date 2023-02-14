// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_LIB_H_INCLUDED
#define LAUF_LIB_H_INCLUDED

#include <lauf/lib/bits.h>
#include <lauf/lib/debug.h>
#include <lauf/lib/fiber.h>
#include <lauf/lib/heap.h>
#include <lauf/lib/int.h>
#include <lauf/lib/limits.h>
#include <lauf/lib/memory.h>
#include <lauf/lib/platform.h>
#include <lauf/lib/test.h>

LAUF_HEADER_START

extern const lauf_runtime_builtin_library* lauf_libs;
extern const size_t                        lauf_libs_count;

LAUF_HEADER_END

#endif // LAUF_LIB_H_INCLUDED

