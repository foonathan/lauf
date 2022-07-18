// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <cstdio>
#include <lauf/asm/builder.h>
#include <lauf/asm/module.h>
#include <lauf/asm/program.h>
#include <lauf/backend/dump.h>
#include <lauf/runtime/value.h>
#include <lauf/vm.h>
#include <lauf/writer.h>

lauf_asm_module* example_module()
{
    auto b   = lauf_asm_create_builder(lauf_asm_default_build_options);
    auto mod = lauf_asm_create_module("test");

    lauf_asm_add_global_zero_data(mod, 1024);
    lauf_asm_add_global_const_data(mod, "hello", 6);
    lauf_asm_add_global_mut_data(mod, "hello", 5);

    auto identity = lauf_asm_add_function(mod, "identity", {1, 1});
    {
        lauf_asm_build(b, mod, identity);

        auto entry = lauf_asm_declare_block(b, {1, 1});
        lauf_asm_build_block(b, entry);
        lauf_asm_inst_panic(b);
        // lauf_asm_inst_return(b);

        lauf_asm_build_finish(b);
    }

    auto main = lauf_asm_add_function(mod, "main", {1, 1});
    {
        lauf_asm_build(b, mod, main);

        auto entry = lauf_asm_declare_block(b, {1, 1});
        lauf_asm_build_block(b, entry);
        lauf_asm_inst_uint(b, 42);
        lauf_asm_inst_pop(b, 1);
        lauf_asm_inst_call(b, identity);
        lauf_asm_inst_return(b);

        lauf_asm_build_finish(b);
    }

    lauf_asm_destroy_builder(b);
    return mod;
}

void dump_module(lauf_asm_module* mod)
{
    auto writer = lauf_create_stdout_writer();
    lauf_backend_dump(writer, lauf_backend_default_dump_options, mod);
    lauf_destroy_writer(writer);
}

template <typename... Input>
void execute(lauf_asm_program* program, Input... inputs)
{
    auto vm = lauf_create_vm(lauf_default_vm_options);

    lauf_runtime_value input[] = {inputs..., lauf_runtime_value()};
    lauf_runtime_value output;
    if (lauf_vm_execute_oneshot(vm, program, input, &output))
        std::printf("result: %lu\n", output.as_sint);

    lauf_destroy_vm(vm);
}

int main()
{
    auto mod = example_module();
    dump_module(mod);

    auto program = lauf_asm_create_program(mod, lauf_asm_find_function_by_name(mod, "main"));
    execute(program, lauf_uint(11));

    lauf_asm_destroy_module(mod);
}

