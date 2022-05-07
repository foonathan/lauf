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
    auto parser = lauf_frontend_text_create_parser();
    lauf_frontend_text_register_builtin(parser, "print", print);
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

        function @fib_iter(1 => 1) {
            block %entry(0 => 3) { # => 0 1 n
                int 0;
                int 1;
                argument 0;

                pick 0;
                branch if_false %exit %loop;
            }
            block %loop(3 => 3) { # a b n => b (a+b) (n-1)
                pick 1;            # => a b n b
                roll 3;            # => b n b a
                call_builtin @add; # => b n (b+a)

                roll 1;                   # => b (b+a) n
                call_builtin @decrement;  # => b (b+a) (n-1)

                pick 0;
                branch if_false %exit %loop;
            }
            block %exit(3 => 1) { # a b n => a
                drop 2;
                return;
            }
        }
    )");
    auto fn  = lauf_module_function_begin(mod)[1];

    auto vm = lauf_vm_create(lauf_default_vm_options);

    lauf_value input = {.as_int = 10};
    lauf_value output;
    lauf_vm_execute(vm, mod, fn, &input, &output);
    std::printf("result: %ld\n", output.as_int);

    lauf_vm_destroy(vm);
    lauf_frontend_text_destroy_parser(parser);
}

