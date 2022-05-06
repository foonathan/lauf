// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <climits>
#include <cstdio>
#include <utility>

#include <lauf/builder.h>
#include <lauf/builtin.h>
#include <lauf/vm.h>

LAUF_BUILTIN_UNARY_OP(print)
{
    std::printf("%lu\n", value.as_int);
    return value;
}

LAUF_BUILTIN_UNARY_OP(increment)
{
    value.as_int++;
    return value;
}
LAUF_BUILTIN_UNARY_OP(decrement)
{
    value.as_int--;
    return value;
}

LAUF_BUILTIN_BINARY_OP(add)
{
    lhs.as_int += rhs.as_int;
    return lhs;
}
LAUF_BUILTIN_BINARY_OP(multiply)
{
    lhs.as_int *= rhs.as_int;
    return lhs;
}

LAUF_BUILTIN_UNARY_OP(is_zero_or_one)
{
    value.as_int = value.as_int == 0 || value.as_int == 1 ? 1 : 0;
    return value;
}

int main()
{
    auto [mod, fn] = [] {
        auto b = lauf_build("test");

        auto fac_decl = lauf_build_declare_function(b, "fac", {1, 1});
        lauf_build_start_function(b, fac_decl);
        {
            lauf_build_argument(b, 0);
            auto if_ = lauf_build_if(b, LAUF_IF_ZERO);
            {
                lauf_build_int(b, 1);
                lauf_build_return(b);
            }
            lauf_build_else(b, &if_);
            {
                lauf_build_argument(b, 0);
                lauf_build_call_builtin(b, decrement);
                lauf_build_call_decl(b, fac_decl);
                lauf_build_argument(b, 0);
                lauf_build_call_builtin(b, multiply);
                lauf_build_return(b);
            }
            lauf_build_end_if(b, &if_);
        }
        auto fn_fac = lauf_build_finish_function(b);

        auto fib_decl = lauf_build_declare_function(b, "fib", {1, 1});
        lauf_build_start_function(b, fib_decl);
        {
            lauf_build_argument(b, 0);
            lauf_build_call_builtin(b, is_zero_or_one);
            auto if_ = lauf_build_if(b, LAUF_IF_NONZERO);
            {
                lauf_build_argument(b, 0);
                lauf_build_return(b);
            }
            lauf_build_else(b, &if_);
            {
                lauf_build_argument(b, 0);
                lauf_build_call_builtin(b, decrement);
                lauf_build_call_decl(b, fib_decl);

                lauf_build_argument(b, 0);
                lauf_build_call_builtin(b, decrement);
                lauf_build_call_builtin(b, decrement);
                lauf_build_call_decl(b, fib_decl);

                lauf_build_call_builtin(b, add);
                lauf_build_return(b);
            }
            lauf_build_end_if(b, &if_);
        }
        auto fn_fib = lauf_build_finish_function(b);

        return std::make_pair(lauf_build_finish(b), fn_fib);
    }();

    auto vm = lauf_vm_create(lauf_default_vm_options);

    lauf_value input = {.as_int = 35};
    lauf_value output;
    lauf_vm_execute(vm, mod, fn, &input, &output);
    std::printf("result: %ld\n", output.as_int);

    lauf_vm_destroy(vm);
}

