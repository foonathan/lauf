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

        function @test(0 => 1) {
            local %x : @Value[2];

            int 42; int 0; local_addr %x; array_element @Value; store_field @Value.0;
            int 11; int 1; local_addr %x; array_element @Value; store_field @Value.0;

            int 0; local_addr %x; array_element @Value; load_field @Value.0;
            return;
        }
    )");
    auto fn      = lauf_module_function_begin(mod)[0];
    auto program = lauf_link_single_module(mod, fn);

    auto vm = lauf_vm_create(lauf_default_vm_options);

    lauf_value input = {.as_sint = 100};
    lauf_value output;
    if (lauf_vm_execute(vm, program, &input, &output))
        std::printf("result: %ld\n", output.as_sint);

    lauf_module_destroy(mod);
    lauf_vm_destroy(vm);
    lauf_frontend_text_destroy_parser(parser);
}

