// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/linker.h>

#include <lauf/detail/stack_allocator.hpp>
#include <lauf/impl/module.hpp>
#include <lauf/impl/program.hpp>

using namespace lauf::_detail;

lauf_program lauf_link_single_module(lauf_module mod, lauf_function entry)
{
    return {mod, entry};
}

