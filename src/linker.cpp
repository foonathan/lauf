// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/linker.h>

#include <lauf/impl/program.hpp>

lauf_program lauf_link_single_module(lauf_module mod, lauf_function entry)
{
    return lauf_program(lauf::_detail::program(mod, entry));
}

