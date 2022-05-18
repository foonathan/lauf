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

LAUF_BUILTIN_UNARY_OPERATION(print, 1, {
    std::printf("%ld\n", value.as_sint);
    *result = value;
})

int main()
{
    auto parser = lauf_frontend_text_create_parser();
    lauf_frontend_text_register_builtin(parser, "print", print());
    lauf_frontend_text_register_builtin(parser, "add",
                                        lauf_sadd_builtin(LAUF_INTEGER_OVERFLOW_WRAP));
    lauf_frontend_text_register_builtin(parser, "sub",
                                        lauf_ssub_builtin(LAUF_INTEGER_OVERFLOW_PANIC));
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
                call @fib;

                load_value %arg; int 2; call_builtin @sub;
                call @fib;

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

        function @fib_tail_call(3 => 1) { # n a b => F_n
            local %a : @Value;
            local %b : @Value;
            block %entry(3 => 1) {
                store_value %b;
                store_value %a;

                pick 0; int 0; call_builtin @cmp;
                branch cmp_eq %is_zero else %next;
            }
            block %next(1 => 1) {
                pick 0; int 1; call_builtin @cmp;
                branch cmp_eq %is_one else %recurse;
            }
            block %is_zero(1 => 1) {
                drop 1;
                load_value %a; return;
            }
            block %is_one(1 => 1) {
                drop 1;
                load_value %b; return;
            }
            block %recurse(1 => 1) {
                int 1; call_builtin @sub;
                load_value %b;
                pick 0; load_value %a; call_builtin @add;
                call @fib_tail_call;
                return;
            }
        }

        function @fib_tail_call_wrapper(1 => 1) {
            block %entry(1 => 1) {
                int 0; int 1;
                call @fib_tail_call;
                return;
            }
        }
    )");
    auto fn      = lauf_module_function_begin(mod)[3];
    auto program = lauf_link_single_module(mod, fn);

    auto vm = lauf_vm_create(lauf_default_vm_options);

    lauf_value input = {.as_sint = 200000000};
    lauf_value output;
    if (lauf_vm_execute(vm, program, &input, &output))
        std::printf("result: %ld\n", output.as_sint);

    lauf_vm_destroy(vm);
    lauf_frontend_text_destroy_parser(parser);
}

