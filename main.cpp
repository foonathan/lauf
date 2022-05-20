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
    lauf_frontend_text_register_builtin(parser, "sadd",
                                        lauf_sadd_builtin(LAUF_INTEGER_OVERFLOW_WRAP));
    lauf_frontend_text_register_builtin(parser, "ssub",
                                        lauf_ssub_builtin(LAUF_INTEGER_OVERFLOW_PANIC));
    lauf_frontend_text_register_builtin(parser, "scmp", lauf_scmp_builtin());
    lauf_frontend_text_register_type(parser, "Int", lauf_native_sint_type());
    lauf_frontend_text_register_type(parser, "Value", &lauf_value_type);
    auto mod     = lauf_frontend_text_cstr(parser, R"(
        module @mod;

        function @fib_recursive(1 => 1) {
            local %arg : @Value;

            store_value %arg;

            load_value %arg; int 1; call_builtin @scmp;
            jump_if cmp_gt %recurse(0);

            load_value %arg;
            return;

        %recurse(0):
            load_value %arg; int 1; call_builtin @ssub;
            call @fib_recursive;

            load_value %arg; int 2; call_builtin @ssub;
            call @fib_recursive;

            call_builtin @sadd;
            return;
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

