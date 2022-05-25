// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <cassert>
#include <cstdio>
#include <lauf/frontend/text.h>
#include <lauf/lib/int.h>
#include <lauf/linker.h>
#include <lauf/vm.h>
#include <lexy/input/file.hpp>
#include <string_view>

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::fprintf(stderr, "usage: %s <file>\n", argv[0]);
        return 1;
    }

    auto file = lexy::read_file<lexy::ascii_encoding>(argv[1]);
    if (!file)
    {
        std::fprintf(stderr, "error: file '%s' not found\n", argv[1]);
        return 1;
    }

    auto parser = lauf_frontend_text_create_parser();
    {
        lauf_frontend_text_register_type(parser, "Value", &lauf_value_type);

        lauf_frontend_text_register_builtin(parser, "sadd",
                                            lauf_sadd_builtin(LAUF_INTEGER_OVERFLOW_PANIC));
        lauf_frontend_text_register_builtin(parser, "ssub",
                                            lauf_ssub_builtin(LAUF_INTEGER_OVERFLOW_PANIC));
        lauf_frontend_text_register_builtin(parser, "scmp", lauf_scmp_builtin());
    }

    auto mod = lauf_frontend_text(parser, argv[1], file.buffer().data(), file.buffer().size());
    if (mod == nullptr)
    {
        std::fprintf(stderr, "error: compilation failure\n");
        return 1;
    }

    auto exit_code = 0;
    auto vm        = lauf_vm_create(lauf_default_vm_options);
    for (auto fn = lauf_module_function_begin(mod); fn != lauf_module_function_end(mod); ++fn)
    {
        auto name = lauf_function_get_name(*fn);
        if (std::string_view(name).find("test_") != 0)
            continue;
        auto should_panic = std::string_view(name).find("panic") != std::string_view::npos;
        if (should_panic)
            lauf_vm_set_panic_handler(vm, [](lauf_panic_info, const char*) {});
        else
            lauf_vm_set_panic_handler(vm, lauf_default_vm_options.panic_handler);

        auto signature = lauf_function_get_signature(*fn);
        assert(signature.input_count == 0 && signature.output_count == 0);

        auto program     = lauf_link_single_module(mod, *fn);
        auto has_paniced = !lauf_vm_execute(vm, program, nullptr, nullptr);
        if (has_paniced != should_panic)
            ++exit_code;
        lauf_program_destroy(program);
    }

    lauf_vm_destroy(vm);
    lauf_frontend_text_destroy_parser(parser);

    return exit_code;
}

