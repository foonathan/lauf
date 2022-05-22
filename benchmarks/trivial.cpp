// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/builder.h>
#include <lauf/lib/int.h>
#include <lauf/linker.h>

lauf_program trivial_add(lauf_builder b)
{
    lauf_build_module(b, "trivial", "");

    auto decl = lauf_declare_function(b, "add", {0, 1});
    lauf_build_function(b, decl);
    lauf_build_int(b, 42);
    lauf_build_int(b, 11);
    lauf_build_call_builtin(b, lauf_sadd_builtin(LAUF_INTEGER_OVERFLOW_PANIC));
    lauf_build_return(b);

    auto fn  = lauf_finish_function(b);
    auto mod = lauf_finish_module(b);
    return lauf_link_single_module(mod, fn);
}

lauf_program trivial_multiply(lauf_builder b)
{
    lauf_build_module(b, "trivial", "");

    auto decl = lauf_declare_function(b, "multiply", {0, 1});
    lauf_build_function(b, decl);
    lauf_build_int(b, 4);
    lauf_build_int(b, 1024);
    lauf_build_call_builtin(b, lauf_smul_builtin(LAUF_INTEGER_OVERFLOW_PANIC));
    lauf_build_int(b, 1024);
    lauf_build_call_builtin(b, lauf_smul_builtin(LAUF_INTEGER_OVERFLOW_PANIC));
    lauf_build_return(b);

    auto fn  = lauf_finish_function(b);
    auto mod = lauf_finish_module(b);
    return lauf_link_single_module(mod, fn);
}

