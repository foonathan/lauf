// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/asm/module.h>
#include <lauf/backend/dump.h>
#include <lauf/writer.h>

lauf_asm_module* example_module()
{
    auto mod = lauf_asm_create_module("test");

    lauf_asm_add_global_zero_data(mod, 1024);
    lauf_asm_add_global_const_data(mod, "hello", 6);
    lauf_asm_add_global_mut_data(mod, "hello", 5);

    lauf_asm_add_function(mod, "fn", {1, 1});
    lauf_asm_add_function(mod, "fn2", {1, 1});

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

