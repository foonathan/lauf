// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <climits>
#include <cstdio>
#include <utility>

#include <lauf/builder.h>
#include <lauf/builtin.h>
#include <lauf/vm.h>

LAUF_BUILTIN_UNARY_OP(increment)
{
    value.as_int++;
    return value;
}

LAUF_BUILTIN_BINARY_OP(add)
{
    lhs.as_int += rhs.as_int;
    return lhs;
}

int main()
{
    auto [mod, fn] = [] {
        auto b = lauf_build("test");

        auto add_decl  = lauf_build_declare_function(b, "add", {2, 1});
        auto test_decl = lauf_build_declare_function(b, "test", {1, 1});

        lauf_build_start_function(b, add_decl);
        lauf_build_argument(b, 0);
        lauf_build_argument(b, 1);
        lauf_build_call_builtin(b, add);
        lauf_build_return(b);
        auto fn_add = lauf_build_finish_function(b);

        lauf_build_start_function(b, test_decl);
        lauf_build_argument(b, 0);
        lauf_build_int(b, 11);
        lauf_build_call(b, fn_add);
        lauf_build_return(b);
        auto fn_test = lauf_build_finish_function(b);

        return std::make_pair(lauf_build_finish(b), fn_test);
    }();

    auto vm = lauf_vm_create(lauf_default_vm_options);

    lauf_value input = {.as_int = 42};
    lauf_value output;
    lauf_vm_execute(vm, mod, fn, &input, &output);
    std::printf("result: %ld\n", output.as_int);

    lauf_vm_destroy(vm);
}

