// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib/int.h>

#include <lauf/aarch64/jit.hpp>

using namespace lauf::aarch64;

namespace
{
bool sadd_wrap(lauf_jit_compiler compiler)
{
    auto [lhs, rhs] = compiler->reg.pop_as_register<2>(compiler->emitter, r_vstack_ptr);
    auto dest       = compiler->reg.push_as_register();
    compiler->emitter.add(dest, lhs, rhs);
    return true;
}

bool ssub_wrap(lauf_jit_compiler compiler)
{
    auto [lhs, rhs] = compiler->reg.pop_as_register<2>(compiler->emitter, r_vstack_ptr);
    auto dest       = compiler->reg.push_as_register();
    compiler->emitter.sub(dest, lhs, rhs);
    return true;
}

bool scmp(lauf_jit_compiler compiler)
{
    auto [lhs, rhs] = compiler->reg.pop_as_register<2>(compiler->emitter, r_vstack_ptr);
    compiler->emitter.cmp(lhs, rhs);
    compiler->emitter.cset(lhs, condition_code::gt);
    compiler->emitter.cset(rhs, condition_code::lt);

    auto dest = compiler->reg.push_as_register();
    compiler->emitter.sub(dest, lhs, rhs);
    return true;
}
} // namespace

bool lauf_try_jit_int(lauf_jit_compiler compiler, lauf_builtin_function fn)
{
    if (fn == lauf_sadd_builtin(LAUF_INTEGER_OVERFLOW_WRAP).impl)
        return sadd_wrap(compiler);
    else if (fn == lauf_ssub_builtin(LAUF_INTEGER_OVERFLOW_WRAP).impl)
        return ssub_wrap(compiler);
    else if (fn == lauf_scmp_builtin().impl)
        return scmp(compiler);
    else
        return false;
}

