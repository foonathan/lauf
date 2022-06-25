// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_IR_REGISTER_ALLOCATOR_HPP_INCLUDED
#define SRC_LAUF_IR_REGISTER_ALLOCATOR_HPP_INCLUDED

#include <lauf/config.h>
#include <lauf/ir/irgen.hpp>

namespace lauf
{
struct machine_register_file
{
    // Number of general purpose registers that are used for function arguments.
    std::uint8_t argument_count;
    // Number of general purpose registers that are caller saved.
    std::uint8_t temporary_count;
    // Number of general purpose registers that are callee saved.
    std::uint8_t persistent_count;
};
} // namespace lauf

namespace lauf
{
struct register_assignment
{
    enum kind
    {
        argument_reg,
        temporary_reg,
        persistent_reg,
    } kind : 2;
    std::uint16_t index : 14;

    constexpr register_assignment() : kind(argument_reg), index(0x3FFF) {}
    constexpr register_assignment(enum kind k, std::uint16_t idx) : kind(k), index(idx) {}

    constexpr explicit operator bool() const
    {
        return index != 0x3FFF;
    }
};

class register_assignments
{
public:
    explicit register_assignments(stack_allocator& alloc, std::size_t virt_register_count)
    : _assignments(alloc, virt_register_count)
    {
        _assignments.resize(virt_register_count);
    }

    register_assignment operator[](register_idx virt_register) const
    {
        return _assignments[std::size_t(virt_register)];
    }

    void assign(register_idx virt_register, register_assignment assgn)
    {
        _assignments[std::size_t(virt_register)] = assgn;
    }

private:
    temporary_array<register_assignment> _assignments;
};

register_assignments register_allocation(stack_allocator& alloc, const machine_register_file& rf,
                                         const ir_function& fn);
} // namespace lauf

#endif // SRC_LAUF_IR_REGISTER_ALLOCATOR_HPP_INCLUDED

