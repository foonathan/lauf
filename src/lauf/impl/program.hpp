// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_IMPL_PROGRAM_HPP_INCLUDED
#define SRC_LAUF_IMPL_PROGRAM_HPP_INCLUDED

#include <lauf/program.h>

namespace lauf::_detail
{
struct program
{
    lauf_module   mod;
    lauf_function entry;

    explicit program(lauf_module mod, lauf_function entry) : mod(mod), entry(entry) {}
    explicit program(lauf_program prog)
    : mod(static_cast<lauf_module>(prog._data[0])), entry(static_cast<lauf_function>(prog._data[1]))
    {}

    explicit operator lauf_program() const
    {
        return {mod, entry};
    }
};
} // namespace lauf::_detail

#endif // SRC_LAUF_IMPL_PROGRAM_HPP_INCLUDED

