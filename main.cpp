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
#include <lauf/lib/int.h>
#include <lauf/linker.h>
#include <lauf/module.h>
#include <lauf/vm.h>

LAUF_BUILTIN_UNARY_OPERATION(print, 1, {
    std::printf("%ld\n", value.as_sint);
    *result = value;
})

constexpr lauf_type_data int_type = {LAUF_NATIVE_LAYOUT_OF(int), 1,
                                     [](const void* object_address, size_t) {
                                         lauf_value result;
                                         result.as_sint
                                             = *static_cast<const lauf_value_sint*>(object_address);
                                         return result;
                                     },
                                     [](void* object_address, size_t, lauf_value value) {
                                         ::new (object_address) lauf_value_sint(value.as_sint);
                                     }};

int main()
{
    auto parser = lauf_frontend_text_create_parser();
    lauf_frontend_text_register_builtin(parser, "print", print());
    lauf_frontend_text_register_builtin(parser, "sadd",
                                        lauf_sadd_builtin(LAUF_INTEGER_OVERFLOW_WRAP));
    lauf_frontend_text_register_builtin(parser, "ssub",
                                        lauf_ssub_builtin(LAUF_INTEGER_OVERFLOW_PANIC));
    lauf_frontend_text_register_builtin(parser, "scmp", lauf_scmp_builtin());
    lauf_frontend_text_register_type(parser, "Int", &int_type);
    lauf_frontend_text_register_type(parser, "Value", &lauf_value_type);
    auto mod     = lauf_frontend_text_cstr(parser, R"(
        module @mod;

        function @test(0 => 1) {
            local %ptr : @Value;
            local %result : @Value;

            layout_of @Value; heap_alloc; store_value %ptr;

            int 42; load_value %ptr; store_field @Value.0;
            load_value %ptr; load_field @Value.0; store_value %result;

            load_value %ptr; free_alloc;

            load_value %result;
            return;
        }
    )");
    auto fn      = lauf_module_function_begin(mod)[0];
    auto program = lauf_link_single_module(mod, fn);

    auto options      = lauf_default_vm_options;
    options.allocator = lauf_vm_malloc_allocator;
    auto vm           = lauf_vm_create(options);

    lauf_value input = {.as_sint = 1};
    lauf_value output;
    if (lauf_vm_execute(vm, program, &input, &output))
        std::printf("result: %ld\n", output.as_sint);

    lauf_program_destroy(program);
    lauf_module_destroy(mod);
    lauf_vm_destroy(vm);
    lauf_frontend_text_destroy_parser(parser);
}

