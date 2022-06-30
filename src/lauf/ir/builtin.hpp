// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_IR_BUILTIN_HPP_INCLUDED
#define SRC_LAUF_IR_BUILTIN_HPP_INCLUDED

#include <lauf/config.h>
#include <lauf/ir/instruction.hpp>
#include <optional>

namespace lauf
{
std::optional<ir_inst> try_irgen_int(lauf_builtin_function fn, register_idx* vstack);
}

#endif // SRC_LAUF_IR_BUILTIN_HPP_INCLUDED

