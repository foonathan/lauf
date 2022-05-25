// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_IMPL_PROGRAM_HPP_INCLUDED
#define SRC_LAUF_IMPL_PROGRAM_HPP_INCLUDED

#include <lauf/program.h>

struct lauf_program_impl
{
    lauf_module   mod;
    lauf_function entry;
    size_t        static_memory_size;

    unsigned char* static_memory()
    {
        return reinterpret_cast<unsigned char*>(this + 1);
    }
};

lauf_program lauf_impl_allocate_program(size_t static_memory_size);

#endif // SRC_LAUF_IMPL_PROGRAM_HPP_INCLUDED

