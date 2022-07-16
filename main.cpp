// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/asm/builder.h>
#include <lauf/asm/module.h>
#include <lauf/backend/dump.h>
#include <lauf/writer.h>

lauf_asm_module* example_module()
{
    auto mod = lauf_asm_create_module("test");

    lauf_asm_add_global_zero_data(mod, 1024);
    lauf_asm_add_global_const_data(mod, "hello", 6);
    lauf_asm_add_global_mut_data(mod, "hello", 5);

    auto fn2 = lauf_asm_add_function(mod, "fn2", {1, 1});

    auto fn = lauf_asm_add_function(mod, "fn", {0, 1});
    {
        auto b = lauf_asm_create_builder(lauf_asm_default_build_options);
        lauf_asm_build(b, mod, fn);

        auto entry = lauf_asm_declare_block(b, {0, 1});

        lauf_asm_build_block(b, entry);
        lauf_asm_inst_uint(b, 1ull << 50);
        lauf_asm_inst_call(b, fn2);
        lauf_asm_inst_return(b);

        lauf_asm_build_finish(b);
        lauf_asm_destroy_builder(b);
    }

    return mod;
}

void dump_module(lauf_asm_module* mod)
{
    auto writer = lauf_create_stdout_writer();
    lauf_backend_dump(writer, lauf_backend_default_dump_options, mod);
    lauf_destroy_writer(writer);
}

int main()
{
    auto mod = example_module();
    dump_module(mod);
    lauf_asm_destroy_module(mod);
}

