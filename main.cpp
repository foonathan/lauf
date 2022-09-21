// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <cstdio>
#include <lauf/asm/builder.h>
#include <lauf/asm/module.h>
#include <lauf/asm/program.h>
#include <lauf/backend/dump.h>
#include <lauf/backend/qbe.h>
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

        function @fib(1 => 1) {
            block %entry(1 => 1) {
                pick 0; sint 2; $lauf.int.scmp; cc lt;
                branch %base(1 => 1) %recurse(1 => 1);
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

        function @fib_local(1 => 1) {
            local %n : $lauf.Value;
            block %entry(1 => 0) {
                pick 0; local_addr %n; store_field $lauf.Value 0;
                sint 2; $lauf.int.scmp; cc lt;
                branch %base(0 => 1) %recurse(0 => 1);
            }
            block %base(0 => 1) {
                local_addr %n; load_field $lauf.Value 0;
                return;
            }
            block %recurse(0 => 1) {
                local_addr %n; load_field $lauf.Value 0; sint 1; $lauf.int.ssub_wrap; call @fib_local;
                local_addr %n; load_field $lauf.Value 0; sint 2; $lauf.int.ssub_wrap; call @fib_local;
                $lauf.int.sadd_wrap;
                return;
            }
        }

        function @fib_generator(0 => 0) {
            block %entry(0 => 0) {
                uint 0;
                pick 0; fiber_suspend (1 => 0);
                uint 1;
                pick 0; fiber_suspend (1 => 0);

                jump %loop(2 => 2);
            }
            block %loop(2 => 2) {
                # a b => b (a+b)
                pick 0; roll 2; $lauf.int.uadd_panic;
                pick 0; fiber_suspend (1 => 0);

                jump %loop(2 => 2);
            }
        }

        function @print_n_fibs(1 => 1) {
            block %entry (1 => 2) {
                function_addr @fib_generator; $lauf.fiber.create;
                roll 1; pick 0; branch %loop(2 => 2) %exit(2 => 1);
            }
            block %loop(2 => 2) {
                # handle n => handle n-1
                pick 1; fiber_resume (0 => 1); $lauf.debug.print; pop 0;
                uint 1; $lauf.int.usub_wrap;

                pick 0; branch %loop(2 => 2) %exit(2 => 1);
            }
            block %exit(2 => 1) {
                # handle 0 => 0
                roll 1; $lauf.fiber.destroy;
                return;
            }
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
    lauf_backend_qbe(writer, lauf_backend_default_qbe_options, mod);
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

    auto program = lauf_asm_create_program(mod, lauf_asm_find_function_by_name(mod, "fib"));
    execute(program, lauf_uint(35));

    lauf_asm_destroy_module(mod);
}

