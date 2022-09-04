// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <cstdio>
#include <lauf/asm/builder.h>
#include <lauf/asm/module.h>
#include <lauf/asm/program.h>
#include <lauf/backend/dump.h>
#include <lauf/frontend/text.h>
#include <lauf/lib.h>
#include <lauf/reader.h>
#include <lauf/runtime/builtin.h>
#include <lauf/runtime/process.h>
#include <lauf/runtime/value.h>
#include <lauf/vm.h>
#include <lauf/writer.h>

lauf_asm_module* example_module()
{
    auto reader = lauf_create_cstring_reader(R"(
        module @test;

        function @fac(1 => 1) {
            block %entry(1 => 1) {
                pick 0;
                branch2 %recurse(1 => 1) %simple(1 => 1);
            }
            block %simple(1 => 1) {
                pop 0;
                sint 1;
                return;
            }
            block %recurse(1 => 1) {
                pick 0;
                sint 1; $lauf.int.ssub_wrap; call @fac;
                $lauf.int.smul_panic;
                return;
            }
        }

        function @fib(1 => 1) {
            block %entry(1 => 1) {
                pick 0; sint 2; $lauf.int.scmp;
                branch3 %base(1 => 1) %recurse(1 => 1) %recurse(1 => 1);
            }
            block %base(1 => 1) {
                return;
            }
            block %recurse(1 => 1) {
                pick 0; sint 1; $lauf.int.ssub_wrap; call @fib;
                roll 1; sint 2; $lauf.int.ssub_wrap; call @fib;
                $lauf.int.sadd_wrap;
                return;
            }
        }

        function @subfiber2(0 => 0) {
            uint 2; $lauf.debug.print; pop 0;
            fiber_suspend;
            $lauf.debug.print_all_cstacks;
            return;
        }

        function @subfiber(0 => 0) {
            uint 1; $lauf.debug.print; pop 0;
            fiber_create;
            fiber_call @subfiber2;
            pop 0;
            return;
        }

        function @main(0 => 1) {
            fiber_create;
            uint 0; $lauf.debug.print; pop 0;
            fiber_call @subfiber; 
            uint 3; $lauf.debug.print; pop 0;
            fiber_resume;
            pop 0;
            uint 0; return;
        }
    )");
    lauf_reader_set_path(reader, "prototype.lauf");

    auto result = lauf_frontend_text(reader, lauf_frontend_default_text_options);
    lauf_destroy_reader(reader);
    return result;
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
    else
        std::puts("panic!");

    lauf_destroy_vm(vm);
}

int main()
{
    auto mod = example_module();
    dump_module(mod);

    auto program = lauf_asm_create_program(mod, lauf_asm_find_function_by_name(mod, "main"));
    execute(program, lauf_uint(35));

    lauf_asm_destroy_module(mod);
}

