// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_AARCH64_JIT_HPP_INCLUDED
#define SRC_LAUF_AARCH64_JIT_HPP_INCLUDED

#include <lauf/aarch64/emitter.hpp>
#include <lauf/aarch64/register_allocator.hpp>
#include <lauf/jit.h>

struct lauf_jit_compiler_impl
{
    lauf::aarch64::emitter            emitter;
    lauf::aarch64::register_allocator reg;
};

bool lauf_try_jit_int(lauf_jit_compiler compiler, lauf_builtin_function fn);

#endif // SRC_LAUF_AARCH64_JIT_HPP_INCLUDED

