// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/asm/module.h>
#include <lauf/backend/dump.h>
#include <lauf/backend/writer.h>

lauf_asm_module* example_module()
{
    auto mod = lauf_asm_create_module("test");
    return mod;
}

void dump_module(lauf_asm_module* mod)
{
    auto writer = lauf_backend_create_stdout_writer();
    lauf_backend_dump(writer, lauf_backend_default_dump_options, mod);
    lauf_backend_destroy_writer(writer);
}

int main()
{
    auto mod = example_module();
    dump_module(mod);
    lauf_asm_destroy_module(mod);
}

