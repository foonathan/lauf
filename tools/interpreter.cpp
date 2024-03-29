// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <cstdio>
#include <lauf/asm/module.h>
#include <lauf/asm/program.h>
#include <lauf/frontend/text.h>
#include <lauf/lib.h>
#include <lauf/reader.h>
#include <lauf/runtime/builtin.h>
#include <lauf/runtime/value.h>
#include <lauf/vm.h>

#include "defer.hpp"

int main(int argc, char* argv[])
{
    lauf_reader* reader = nullptr;
    {
        if (argc < 2)
        {
            reader = lauf_create_stdin_reader();
        }
        else
        {
            reader = lauf_create_file_reader(argv[1]);
            if (reader == nullptr)
            {
                std::fprintf(stderr, "input file '%s' not found\n", argv[1]);
                return 1;
            }
        }
    }
    LAUF_DEFER_EXPR(lauf_destroy_reader(reader));

    auto mod = lauf_frontend_text(reader, lauf_frontend_default_text_options);
    if (mod == nullptr)
        return 2;
    LAUF_DEFER_EXPR(lauf_asm_destroy_module(mod));

    auto main = lauf_asm_find_function_by_name(mod, "main");
    if (main == nullptr)
    {
        std::fprintf(stderr, "main function not found\n");
        return 3;
    }
    if (auto sig = lauf_asm_function_signature(main); sig.input_count != 0 || sig.output_count > 1)
    {
        std::fprintf(stderr, "invalid signature of main function\n");
        return 3;
    }

    auto vm = lauf_create_vm(lauf_default_vm_options);
    LAUF_DEFER_EXPR(lauf_destroy_vm(vm));

    auto               program   = lauf_asm_create_program(mod, main);
    lauf_runtime_value exit_code = {0};
    if (!lauf_vm_execute_oneshot(vm, program, nullptr, &exit_code))
        return 4;

    return int(exit_code.as_sint);
}

