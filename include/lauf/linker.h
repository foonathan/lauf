// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_LINKER_H_INCLUDED
#define LAUF_LINKER_H_INCLUDED

#include <lauf/config.h>
#include <lauf/module.h>
#include <lauf/program.h>

LAUF_HEADER_START

lauf_program lauf_link_single_module(lauf_module mod, lauf_function entry);

LAUF_HEADER_END

#endif // LAUF_LINKER_H_INCLUDED

