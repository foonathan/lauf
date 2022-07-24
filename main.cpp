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

LAUF_RUNTIME_BUILTIN(builtin_add, 2, 1, LAUF_RUNTIME_BUILTIN_NO_PROCESS, "add", nullptr)
{
    vstack_ptr[1].as_sint = vstack_ptr[1].as_sint + vstack_ptr[0].as_sint;
    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(builtin_sub, 2, 1, LAUF_RUNTIME_BUILTIN_NO_PROCESS, "sub", &builtin_add)
{
    vstack_ptr[1].as_sint = vstack_ptr[1].as_sint - vstack_ptr[0].as_sint;
    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(builtin_mul, 2, 1, LAUF_RUNTIME_BUILTIN_NO_PROCESS, "mul", &builtin_sub)
{
    vstack_ptr[1].as_sint = vstack_ptr[1].as_sint * vstack_ptr[0].as_sint;
    ++vstack_ptr;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

const lauf_runtime_builtin_library my_lib = {"my", &builtin_mul};

const lauf_runtime_builtin_library builtins[]         = {lauf_lib_debug, lauf_lib_test, my_lib};
constexpr size_t                   builtin_libs_count = 3;

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
                sint 1; $my.sub; call @fac;
                $my.mul;
                return;
            }
        }

        function @fib(1 => 1) {
            local %arg : $lauf.Value;
            block %entry(1 => 1) {
                pick 0; sint 2; $my.sub;
                branch3 %base(1 => 1) %recurse(1 => 1) %recurse(1 => 1);
            }
            block %base(1 => 1) {
                return;
            }
            block %recurse(1 => 1) {
                pick 0; sint 1; $my.sub; call @fib;
                roll 1; sint 2; $my.sub; call @fib;
                $my.add;
                return;
            }
        }

        function @test(0 => 1) {
            local %foo : (8, 8);
            uint 42;
            local_addr %foo;
            store_field $lauf.Value 0;

            local_addr %foo;
            load_field $lauf.Value 0;
            $lauf.debug.print;

            return;
        }
    )");

    auto opts               = lauf_frontend_default_text_options;
    opts.builtin_libs       = builtins;
    opts.builtin_libs_count = builtin_libs_count;
    auto result             = lauf_frontend_text(reader, opts);

    lauf_destroy_reader(reader);
    return result;
}

void dump_module(lauf_asm_module* mod)
{
    auto writer = lauf_create_stdout_writer();

    auto opts               = lauf_backend_default_dump_options;
    opts.builtin_libs       = builtins;
    opts.builtin_libs_count = builtin_libs_count;
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

    auto program = lauf_asm_create_program(mod, lauf_asm_find_function_by_name(mod, "test"));
    execute(program, lauf_uint(35));

    lauf_asm_destroy_module(mod);
}

