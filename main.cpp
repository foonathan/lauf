// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <climits>
#include <cstdio>
#include <new>
#include <utility>

#include <lauf/builder.h>
#include <lauf/builtin.h>
#include <lauf/frontend/text.h>
#include <lauf/linker.h>
#include <lauf/module.h>
#include <lauf/vm.h>

LAUF_BUILTIN_UNARY_OP(print)
{
    std::printf("%lu\n", value.as_int);
    return value;
}
LAUF_BUILTIN_UNARY_OP(print_str)
{
    std::printf("%s\n", static_cast<const char*>(value.as_ptr));
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

LAUF_NATIVE_PRIMITIVE_TYPE(type_int, lauf_value_int, as_int);

int main()
{
    auto parser = lauf_frontend_text_create_parser();
    lauf_frontend_text_register_builtin(parser, "print", print);
    lauf_frontend_text_register_builtin(parser, "print_str", print_str);
    lauf_frontend_text_register_builtin(parser, "is_zero_or_one", is_zero_or_one);
    lauf_frontend_text_register_builtin(parser, "decrement", decrement);
    lauf_frontend_text_register_builtin(parser, "add", add);
    lauf_frontend_text_register_type(parser, "Int", &type_int);
    lauf_frontend_text_register_type(parser, "Value", &lauf_value_type);
    auto mod     = lauf_frontend_text_cstr(parser, R"(
        module @mod;

        function @fib(1 => 1) {
            local %arg : @Value;
            block %entry(1 => 0) {
                store_value %arg;
                load_value %arg; call_builtin @is_zero_or_one;
                branch if_true %base else %recurse;
            }
            block %base(0 => 1) {
                load_value %arg;
                return;
            }
            block %recurse(0 => 1) {
                load_value %arg;
                call_builtin @decrement;
                recurse;

                load_value %arg;
                call_builtin @decrement;
                call_builtin @decrement;
                recurse;

                call_builtin @add;
                return;
            }
        }

        function @fib_iter(1 => 1) {
            local %a : @Int;
            local %b : @Int;
            block %entry(1 => 1) {
                int 0; local_addr %a; store_field @Int.0;
                int 1; local_addr %b; store_field @Int.0;

                pick 0; branch if_false %exit else %loop;
            }
            block %loop(1 => 1) { # n => (n-1)
                local_addr %b; load_field @Int.0; # => b

                pick 0;                           # => b b
                local_addr %a; load_field @Int.0; # => b b a
                call_builtin @add;                # => b (b+a)

                local_addr %b; store_field @Int.0;
                local_addr %a; store_field @Int.0;

                call_builtin @decrement;
                pick 0; branch if_false %exit else %loop;
            }
            block %exit(1 => 1) {
                drop 1;
                local_addr %a; load_field @Int.0;
                return;
            }
        }

        const @data = "hello", 65, 0;

        function @test(1 => 1) {
            block %entry(1 => 1) {
                ptr @data;
                assert if_true;
                int 42;
                return;
            }
        }
    )");
    auto fn      = lauf_module_function_begin(mod)[2];
    auto program = lauf_link_single_module(mod, fn);

    auto vm = lauf_vm_create(lauf_default_vm_options);

    lauf_value input = {.as_int = 10};
    lauf_value output;
    if (lauf_vm_execute(vm, program, &input, &output))
        std::printf("result: %ld\n", output.as_int);

    lauf_vm_destroy(vm);
    lauf_frontend_text_destroy_parser(parser);
}

