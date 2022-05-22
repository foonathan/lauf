// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/builder.h>
#include <lauf/lib/int.h>
#include <lauf/linker.h>

lauf_program recursive_fib(lauf_builder b)
{
    lauf_build_module(b, "fib", "");

    auto fib  = lauf_declare_function(b, "fib", {1, 1});
    auto main = lauf_declare_function(b, "main", {0, 1});

    lauf_build_function(b, fib);
    {
        auto recurse = lauf_declare_label(b, 0);

        // arg == 1
        auto arg = lauf_build_local_variable(b, lauf_value_type.layout);
        lauf_build_store_value(b, arg);
        lauf_build_load_value(b, arg);
        lauf_build_int(b, 1);
        lauf_build_call_builtin(b, lauf_scmp_builtin());

        lauf_build_jump_if(b, LAUF_CMP_GT, recurse);
        {
            // return arg
            lauf_build_load_value(b, arg);
            lauf_build_return(b);
        }

        lauf_place_label(b, recurse);
        {
            // fib(n - 1)
            lauf_build_load_value(b, arg);
            lauf_build_int(b, 1);
            lauf_build_call_builtin(b, lauf_ssub_builtin(LAUF_INTEGER_OVERFLOW_PANIC));
            lauf_build_call(b, fib);

            // fib(n - 2)
            lauf_build_load_value(b, arg);
            lauf_build_int(b, 2);
            lauf_build_call_builtin(b, lauf_ssub_builtin(LAUF_INTEGER_OVERFLOW_PANIC));
            lauf_build_call(b, fib);

            // +
            lauf_build_call_builtin(b, lauf_sadd_builtin(LAUF_INTEGER_OVERFLOW_PANIC));
            lauf_build_return(b);
        }

        lauf_finish_function(b);
    }

    lauf_build_function(b, main);
    {
        lauf_build_int(b, 35);
        lauf_build_call(b, fib);
        lauf_build_return(b);
    }
    auto fn = lauf_finish_function(b);

    auto mod = lauf_finish_module(b);
    return lauf_link_single_module(mod, fn);
}

lauf_program iterative_fib(lauf_builder b)
{
    lauf_build_module(b, "fib", "");

    auto fib  = lauf_declare_function(b, "fib", {1, 1});
    auto main = lauf_declare_function(b, "main", {0, 1});

    lauf_build_function(b, fib);
    {
        auto var_a = lauf_build_local_variable(b, lauf_value_type.layout);
        auto var_b = lauf_build_local_variable(b, lauf_value_type.layout);

        auto loop = lauf_declare_label(b, 1);
        auto exit = lauf_declare_label(b, 0);

        // a := 0
        lauf_build_int(b, 0);
        lauf_build_store_value(b, var_a);
        // b := 1
        lauf_build_int(b, 1);
        lauf_build_store_value(b, var_b);

        // n == 0?
        lauf_build_pick(b, 0);
        lauf_build_int(b, 0);
        lauf_build_call_builtin(b, lauf_scmp_builtin());
        lauf_build_jump_if(b, LAUF_CMP_EQ, exit);

        lauf_place_label(b, loop);
        {
            lauf_build_load_value(b, var_b);

            // b => b (b + a)
            lauf_build_pick(b, 0);
            lauf_build_load_value(b, var_a);
            lauf_build_call_builtin(b, lauf_sadd_builtin(LAUF_INTEGER_OVERFLOW_PANIC));

            lauf_build_store_value(b, var_b);
            lauf_build_store_value(b, var_a);

            // n - 1
            lauf_build_int(b, 1);
            lauf_build_call_builtin(b, lauf_ssub_builtin(LAUF_INTEGER_OVERFLOW_PANIC));

            // n == 0?
            lauf_build_pick(b, 0);
            lauf_build_int(b, 0);
            lauf_build_call_builtin(b, lauf_scmp_builtin());
            lauf_build_jump_if(b, LAUF_CMP_NE, loop);
        }

        lauf_place_label(b, exit);
        {
            lauf_build_drop(b, 1);
            lauf_build_load_value(b, var_a);
            lauf_build_return(b);
        }

        lauf_finish_function(b);
    }

    lauf_build_function(b, main);
    {
        lauf_build_int(b, 90);
        lauf_build_call(b, fib);
        lauf_build_return(b);
    }
    auto fn = lauf_finish_function(b);

    auto mod = lauf_finish_module(b);
    return lauf_link_single_module(mod, fn);
}

