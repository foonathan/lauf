// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <cstdio>
#include <lauf/asm/builder.h>
#include <lauf/asm/module.h>
#include <lauf/asm/program.h>
#include <lauf/backend/dump.h>
#include <lauf/frontend/text.h>
#include <lauf/lib/debug.h>
#include <lauf/reader.h>
#include <lauf/runtime/builtin.h>
#include <lauf/runtime/process.h>
#include <lauf/runtime/value.h>
#include <lauf/vm.h>
#include <lauf/writer.h>

const lauf_runtime_builtin_function builtin_print
    = {[](lauf_runtime_process*, const lauf_runtime_value* input, lauf_runtime_value* output) {
           std::printf("print: %lu\n", input->as_uint);
           output[0] = input[0];
           return true;
       },
       1,
       1,
       LAUF_RUNTIME_BUILTIN_NO_PROCESS,
       "lauf.print",
       nullptr};

lauf_asm_module* example_module()
{
    auto reader = lauf_create_cstring_reader(R"(
        module @test;

        global const @msg = "hello", 0;

        function @identity(1 => 1) {
            block %entry(1 => 1) {
                $lauf.debug.print_vstack;
                $lauf.debug.print_cstack;
                $lauf.debug.break;
                return;
            }
        }

        function @main(1 => 1) {
            block %entry(1 => 1) {
                uint 0;
                call @identity;
                pop 0;
                return;
            }
        }

    )");

    auto opts     = lauf_frontend_default_text_options;
    opts.builtins = lauf_lib_debug;
    auto result   = lauf_frontend_text(reader, opts);

    lauf_destroy_reader(reader);
    return result;
}

void dump_module(lauf_asm_module* mod)
{
    auto writer = lauf_create_stdout_writer();

    auto opts     = lauf_backend_default_dump_options;
    opts.builtins = lauf_lib_debug;
    lauf_backend_dump(writer, opts, mod);

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
    else
        std::puts("panic!\n");

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

