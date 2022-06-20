// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_IR_CFG_HPP_INCLUDED
#define SRC_LAUF_IR_CFG_HPP_INCLUDED

#include <cassert>
#include <lauf/bc/bytecode.hpp>
#include <lauf/config.h>
#include <lauf/support/temporary_array.hpp>

namespace lauf
{
class control_flow_graph
{
public:
    class terminator
    {
    public:
        enum kind : unsigned char
        {
            // No successor.
            exit,
            // Single, unconditional successor.
            jump,
            // True and false successor.
            branch,
        };

        terminator() noexcept : _kind(exit) {}
        explicit terminator(std::size_t dest) noexcept : _kind(jump), _first(dest) {}
        explicit terminator(condition_code cc, std::size_t if_true, std::size_t if_false) noexcept
        : _kind(branch), _cc(cc), _first(if_true), _second(if_false)
        {}

        operator kind() const noexcept
        {
            return _kind;
        }

        condition_code cc() const noexcept
        {
            assert(_kind == branch);
            return _cc;
        }

        std::size_t target() const noexcept
        {
            assert(_kind == jump);
            return _first;
        }

        std::size_t if_true() const noexcept
        {
            assert(_kind == branch);
            return _first;
        }
        std::size_t if_false() const noexcept
        {
            assert(_kind == branch);
            return _second;
        }

    private:
        kind           _kind;
        condition_code _cc;
        std::size_t    _first, _second;

        friend control_flow_graph build_cfg(stack_allocator& alloc, lauf_function fn);
    };

    // Does not include terminator instruction.
    // It does include panic/return in case of exit.
    class basic_block
    {
    public:
        explicit basic_block(lauf_vm_instruction* begin, lauf_vm_instruction* end,
                             control_flow_graph::terminator term) noexcept
        : _begin(begin), _end(end), _term(term)
        {}

        lauf_vm_instruction* begin() const noexcept
        {
            return _begin;
        }
        lauf_vm_instruction* end() const noexcept
        {
            return _end;
        }

        control_flow_graph::terminator terminator() const noexcept
        {
            return _term;
        }

    private:
        lauf_vm_instruction*           _begin;
        lauf_vm_instruction*           _end;
        control_flow_graph::terminator _term;

        friend control_flow_graph build_cfg(stack_allocator& alloc, lauf_function fn);
    };

    const basic_block& entry() const noexcept
    {
        return _bbs[0];
    }

    const basic_block& operator[](std::size_t idx) const noexcept
    {
        return _bbs[idx];
    }

private:
    explicit control_flow_graph(stack_allocator& alloc) : _bbs(alloc, 8) {}

    temporary_array<basic_block> _bbs;

    friend control_flow_graph build_cfg(stack_allocator& alloc, lauf_function fn);
};

control_flow_graph build_cfg(stack_allocator& alloc, lauf_function fn);
} // namespace lauf

#endif // SRC_LAUF_IR_CFG_HPP_INCLUDED

