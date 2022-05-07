// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <climits>
#include <cstdio>
#include <utility>

#include <lauf/builder.h>
#include <lauf/builtin.h>
#include <lauf/frontend/text.h>
#include <lauf/module.h>
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
#if 0
    auto [mod, fn] = [] {
        auto mod = lauf_build_module("test");

        auto test       = lauf_build_function(mod, "test", {1, 1});
        auto entry      = lauf_build_entry_block(test, {1, 0});
        auto if_zero    = lauf_build_block(test, {0, 1});
        auto if_nonzero = lauf_build_block(test, {0, 1});
        auto exit       = lauf_build_block(test, {1, 1});

        lauf_build_argument(entry, 0);
        lauf_finish_block_branch(entry, LAUF_IF_ZERO, if_zero, if_nonzero);

        lauf_build_int(if_zero, 1);
        lauf_finish_block_jump(if_zero, exit);

        lauf_build_argument(if_nonzero, 0);
        lauf_build_argument(if_nonzero, 0);
        lauf_build_call_builtin(if_nonzero, decrement);
        lauf_build_recurse(if_nonzero);
        lauf_build_call_builtin(if_nonzero, multiply);
        lauf_finish_block_jump(if_nonzero, exit);

        lauf_finish_block_return(exit);

        auto fn_test = lauf_finish_function(test);
        return std::make_pair(lauf_finish_module(mod), fn_test);
    }();
#endif

    auto parser = lauf_frontend_text_create_parser();
    lauf_frontend_text_register_builtin(parser, "is_zero_or_one", is_zero_or_one);
    lauf_frontend_text_register_builtin(parser, "decrement", decrement);
    lauf_frontend_text_register_builtin(parser, "add", add);
    auto mod = lauf_frontend_text_cstr(parser, R"(
        module @mod;

        function @fib(1 => 1) {
            block %entry(0 => 0) {
                argument 0;
                call_builtin @is_zero_or_one;
                branch if_true %base %recurse;
            }
            block %base(0 => 1) {
                argument 0;
                return;
            }
            block %recurse(0 => 1) {
                argument 0;
                call_builtin @decrement;
                recurse;

                argument 0;
                call_builtin @decrement;
                call_builtin @decrement;
                recurse;

                call_builtin @add;
                return;
            }
        }
    )");
    auto fn  = lauf_module_function_begin(mod)[0];

    auto vm = lauf_vm_create(lauf_default_vm_options);

    lauf_value input = {.as_int = 10};
    lauf_value output;
    lauf_vm_execute(vm, mod, fn, &input, &output);
    std::printf("result: %ld\n", output.as_int);

    lauf_vm_destroy(vm);
    lauf_frontend_text_destroy_parser(parser);
}

