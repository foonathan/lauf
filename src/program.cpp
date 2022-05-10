// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/program.h>

#include <lauf/impl/module.hpp>
#include <lauf/impl/program.hpp>

void lauf_program_destroy(lauf_program) {}

lauf_signature lauf_program_get_signature(lauf_program prog)
{
    auto fn = lauf_program_get_entry_function(prog);
    return lauf_signature{fn->input_count, fn->output_count};
}

lauf_function lauf_program_get_entry_function(lauf_program prog)
{
    return lauf::_detail::program(prog).entry;
}

