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

namespace lauf::aarch64
{
// Calling convention: x0 is parent ip, x1 is vstack_ptr, x2 is frame_ptr, x3 is process.
// Register x19-x29 are used for the value stack.
constexpr auto r_ip         = register_(0);
constexpr auto r_vstack_ptr = register_(1);
constexpr auto r_frame_ptr  = register_(2);
constexpr auto r_process    = register_(3);
} // namespace lauf::aarch64

bool lauf_try_jit_int(lauf_jit_compiler compiler, lauf_builtin_function fn);

#endif // SRC_LAUF_AARCH64_JIT_HPP_INCLUDED

