// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <utility>

#include <lauf/builder.h>
#include <lauf/builtin.h>
#include <lauf/frontend/text.h>
#include <lauf/jit.h>
#include <lauf/lib/int.h>
#include <lauf/linker.h>
#include <lauf/module.h>
#include <lauf/vm.h>

LAUF_BUILTIN_UNARY_OPERATION(print, 1, {
    std::printf("%d\n", value.as_address.allocation);
    *result = value;
})

int main()
{
    auto parser = lauf_frontend_text_create_parser();
    lauf_frontend_text_register_builtin(parser, "print", print());
    lauf_frontend_text_register_builtin(parser, "sadd",
                                        lauf_sadd_builtin(LAUF_INTEGER_OVERFLOW_WRAP));
    lauf_frontend_text_register_builtin(parser, "ssub",
                                        lauf_ssub_builtin(LAUF_INTEGER_OVERFLOW_WRAP));
    lauf_frontend_text_register_builtin(parser, "usub",
                                        lauf_usub_builtin(LAUF_INTEGER_OVERFLOW_PANIC));
    lauf_frontend_text_register_builtin(parser, "scmp", lauf_scmp_builtin());
    lauf_frontend_text_register_type(parser, "Value", &lauf_value_type);
    auto mod     = lauf_frontend_text_cstr(parser, R"(
        module @mod;

        function @test(0 => 2) {
            uint 1;
            call @inner;
            return;
        }

        function @inner(0 => 1) {
            uint 3;
            uint 1;
            $usub;
            return;
        }

        function @fib_recursive(1 => 1) {
            local %arg : $Value;

            store_value %arg;

            load_value %arg; sint 2; $scmp;
            jump_if cmp_ge %recurse;

            load_value %arg;
            return;

        label %recurse:
            load_value %arg; sint 1; $ssub;
            call @fib_recursive;

            load_value %arg; sint 2; $ssub;
            call @fib_recursive;

            $sadd;
            return;
        }
    )");
    auto fn      = lauf_module_function_begin(mod)[0];
    auto program = lauf_link_single_module(mod, fn);

    auto options      = lauf_default_vm_options;
    options.allocator = lauf_vm_malloc_allocator;
    auto vm           = lauf_vm_create(options);

    auto compiler = lauf_vm_jit_compiler(vm);
    lauf_jit_compile(compiler, fn);
    lauf_jit_compile(compiler, lauf_module_function_begin(mod)[1]);

    lauf_value input[2] = {{.as_sint = 1}, {.as_sint = 2}};
    lauf_value output[2];
    if (lauf_vm_execute(vm, program, input, output))
        std::printf("result: %ld %ld\n", output[0].as_sint, output[1].as_sint);

    lauf_program_destroy(program);
    lauf_module_destroy(mod);
    lauf_vm_destroy(vm);
    lauf_frontend_text_destroy_parser(parser);
}

