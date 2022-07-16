// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_AARCH64_REGISTER_HPP_INCLUDED
#define SRC_LAUF_AARCH64_REGISTER_HPP_INCLUDED

#include <lauf/aarch64/assembler.hpp>
#include <lauf/ir/register_allocator.hpp>

// NOTE: Changing register constants here requires updating them in the assembler file as well.

namespace lauf::aarch64
{
constexpr auto register_file = machine_register_file{
    .argument_count   = 8, // X0-X7
    .temporary_count  = 7, // X9-X15
    .persistent_count = 10 // X19-X28
};

constexpr register_nr reg_argument(std::uint8_t nr)
{
    assert(nr < register_file.argument_count);
    return register_nr(nr);
}

constexpr register_nr reg_temporary(std::uint8_t nr)
{
    assert(nr < register_file.temporary_count);
    return register_nr(9 + nr);
}

constexpr register_nr reg_persistent(std::uint8_t nr)
{
    assert(nr < register_file.persistent_count);
    return register_nr(19 + nr);
}

constexpr register_nr reg_of(register_assignment assgn)
{
    switch (assgn.kind)
    {
    case register_assignment::argument_reg:
        return reg_argument(assgn.index);
    case register_assignment::temporary_reg:
        return reg_temporary(assgn.index);
    case register_assignment::persistent_reg:
        return reg_persistent(assgn.index);
    }
}

// Register that holds state during JIT execution.
//
// For normal execution, this is the process pointer.
// During panic propagation, it is null.
//
// We re-purpose X8, which is normally used to pass a pointer for bigger return values, but we don't
// need that.
constexpr auto reg_jit_state = register_nr{8};
} // namespace lauf::aarch64

#endif // SRC_LAUF_AARCH64_REGISTER_HPP_INCLUDED

