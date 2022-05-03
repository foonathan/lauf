// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_DETAIL_BYTECODE_HPP_INCLUDED
#define SRC_DETAIL_BYTECODE_HPP_INCLUDED

#include <cstddef>

namespace lauf
{
enum class op : unsigned char
{
    // push ConstantIndex16
    push,
    // pop Count16
    pop,
};
} // namespace lauf

#endif // SRC_DETAIL_BYTECODE_HPP_INCLUDED

