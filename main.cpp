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

#include "src/lauf/ir/irdump.hpp"

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
                                        lauf_ssub_builtin(LAUF_INTEGER_OVERFLOW_WRAP));
    lauf_frontend_text_register_builtin(parser, "usub",
                                        lauf_usub_builtin(LAUF_INTEGER_OVERFLOW_PANIC));
    lauf_frontend_text_register_builtin(parser, "scmp", lauf_scmp_builtin());
    lauf_frontend_text_register_type(parser, "Value", &lauf_value_type);
    auto mod     = lauf_frontend_text_cstr(parser, R"(
        module @mod;

        function @test(1 => 2) {
            pick 0;
            jump_if is_false %foo;
            sint 13;
            return;

        label %foo(1):
            sint 11;
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

    lauf::memory_stack    stack;
    lauf::stack_allocator alloc(stack);
    auto                  ir    = lauf::irgen(alloc, fn);
    auto                  assgn = lauf::register_allocation(alloc, {8, 8, 14}, ir);
    std::puts(lauf::irdump(ir, &assgn).c_str());

    auto compiler = lauf_vm_jit_compiler(vm);
    lauf_jit_compile(compiler, fn);

#if 0
    lauf_value input = {.as_sint = 35};
    lauf_value output;
    if (lauf_vm_execute(vm, program, &input, &output))
        std::printf("result: %ld\n", output.as_sint);
#endif

    lauf_program_destroy(program);
    lauf_module_destroy(mod);
    lauf_vm_destroy(vm);
    lauf_frontend_text_destroy_parser(parser);
}

