// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_IR_IRGEN_HPP_INCLUDED
#define SRC_LAUF_IR_IRGEN_HPP_INCLUDED

#include <lauf/config.h>
#include <lauf/ir/instruction.hpp>
#include <lauf/support/temporary_array.hpp>

namespace lauf
{
struct ir_inst_range
{
    const ir_inst *_begin, *_end;

    const ir_inst* begin() const noexcept
    {
        return _begin;
    }
    const ir_inst* end() const noexcept
    {
        return _end;
    }
};

class ir_function
{
    struct basic_block
    {
        std::size_t begin, end;
    };

public:
    ir_inst_range entry_block() const noexcept
    {
        return block(block_idx(0));
    }
    ir_inst_range block(block_idx idx) const noexcept
    {
        auto bb = _blocks[static_cast<std::size_t>(idx)];
        return {_instructions.data() + bb.begin, _instructions.data() + bb.end};
    }

    std::size_t block_count() const noexcept
    {
        return _blocks.size();
    }

    ir_inst_range instructions() const noexcept
    {
        return {_instructions.begin(), _instructions.end()};
    }

private:
    explicit ir_function(stack_allocator& alloc) : _instructions(alloc, 1024), _blocks(alloc, 16) {}

    temporary_array<ir_inst>     _instructions;
    temporary_array<basic_block> _blocks;

    friend ir_function irgen(stack_allocator& alloc, lauf_function fn);
};

ir_function irgen(stack_allocator& alloc, lauf_function fn);
} // namespace lauf

#endif // SRC_LAUF_IR_IRGEN_HPP_INCLUDED

