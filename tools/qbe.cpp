// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <cstdio>
#include <lauf/asm/module.h>
#include <lauf/backend/qbe.h>
#include <lauf/frontend/text.h>
#include <lauf/reader.h>
#include <lauf/writer.h>
#include <string>

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

    auto writer = lauf_create_stdout_writer();
    LAUF_DEFER_EXPR(lauf_destroy_writer(writer));
    lauf_backend_qbe(writer, lauf_backend_default_qbe_options, mod);
}

