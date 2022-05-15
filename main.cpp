// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <climits>
#include <cstdio>
#include <new>
#include <utility>

#include <lauf/builder.h>
#include <lauf/builtin.h>
#include <lauf/frontend/text.h>
#include <lauf/lib/int.h>
#include <lauf/linker.h>
#include <lauf/module.h>
#include <lauf/vm.h>

LAUF_BUILTIN_UNARY_OP(print)
{
    std::printf("%lu\n", value.as_sint);
    return value;
}
LAUF_BUILTIN_UNARY_OP(print_str)
{
    std::printf("%s\n", static_cast<const char*>(value.as_ptr));
    return value;
}

int main()
{
    auto parser = lauf_frontend_text_create_parser();
    lauf_frontend_text_register_builtin(parser, "print", print);
    lauf_frontend_text_register_builtin(parser, "print_str", print_str);
    lauf_frontend_text_register_builtin(parser, "add",
                                        lauf_sadd_builtin(LAUF_INTEGER_OVERFLOW_WRAP));
    lauf_frontend_text_register_builtin(parser, "sub",
                                        lauf_ssub_builtin(LAUF_INTEGER_OVERFLOW_WRAP));
    lauf_frontend_text_register_builtin(parser, "cmp", lauf_scmp_builtin());
    lauf_frontend_text_register_type(parser, "Int", lauf_native_sint_type());
    lauf_frontend_text_register_type(parser, "Value", &lauf_value_type);
    auto mod     = lauf_frontend_text_cstr(parser, R"(
        module @mod;

        function @fib(1 => 1) {
            local %arg : @Value;
            block %entry(1 => 0) {
                store_value %arg;

                load_value %arg; int 1; call_builtin @cmp;
                branch cmp_le %base else %recurse;
            }
            block %base(0 => 1) {
                load_value %arg;
                return;
            }
            block %recurse(0 => 1) {
                load_value %arg; int 1; call_builtin @sub;
                recurse;

                load_value %arg; int 2; call_builtin @sub;
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

                pick 0; branch is_false %exit else %loop;
            }
            block %loop(1 => 1) { # n => (n-1)
                local_addr %b; load_field @Int.0; # => b

                pick 0;                           # => b b
                local_addr %a; load_field @Int.0; # => b b a
                call_builtin @add;                # => b (b+a)

                local_addr %b; store_field @Int.0;
                local_addr %a; store_field @Int.0;

                int 1; call_builtin @sub;
                pick 0; branch is_false %exit else %loop;
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
                panic_if is_true;
                int 42;
                return;
            }
        }
    )");
    auto fn      = lauf_module_function_begin(mod)[0];
    auto program = lauf_link_single_module(mod, fn);

    auto vm = lauf_vm_create(lauf_default_vm_options);

    lauf_value input = {.as_sint = 35};
    lauf_value output;
    if (lauf_vm_execute(vm, program, &input, &output))
        std::printf("result: %ld\n", output.as_sint);

    lauf_vm_destroy(vm);
    lauf_frontend_text_destroy_parser(parser);
}

