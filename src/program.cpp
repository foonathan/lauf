// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/program.h>

#include <lauf/impl/module.hpp>
#include <lauf/impl/program.hpp>

lauf_program lauf_impl_allocate_program()
{
    auto memory = ::operator new(sizeof(lauf_program_impl));
    return static_cast<lauf_program>(memory);
}

void lauf_program_destroy(lauf_program prog)
{
    ::operator delete(prog);
}

lauf_signature lauf_program_get_signature(lauf_program prog)
{
    auto fn = prog->entry;
    return lauf_signature{fn->input_count, fn->output_count};
}

lauf_function lauf_program_get_entry_function(lauf_program prog)
{
    return prog->entry;
}

