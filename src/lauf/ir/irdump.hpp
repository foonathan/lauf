// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_IR_IRDUMP_HPP_INCLUDED
#define SRC_LAUF_IR_IRDUMP_HPP_INCLUDED

#include <lauf/ir/irgen.hpp>
#include <string>

namespace lauf
{
std::string irdump(const ir_function& fn);
}

#endif // SRC_LAUF_IR_IRDUMP_HPP_INCLUDED

