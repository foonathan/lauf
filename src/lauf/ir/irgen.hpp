// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_IR_IRGEN_HPP_INCLUDED
#define SRC_LAUF_IR_IRGEN_HPP_INCLUDED

#include <lauf/config.h>
#include <lauf/ir/instruction.hpp>
#include <lauf/support/temporary_array.hpp>
#include <optional>

namespace lauf
{
struct ir_block_range
{
    // Just enough to be used in a range-based for loop.
    struct iterator
    {
        std::uint16_t _value;

        block_idx operator*() const noexcept
        {
            return block_idx(_value);
        }

        iterator& operator++()
        {
            ++_value;
            return *this;
        }

        friend bool operator==(iterator lhs, iterator rhs)
        {
            return lhs._value == rhs._value;
        }
        friend bool operator!=(iterator lhs, iterator rhs)
        {
            return lhs._value != rhs._value;
        }
    };

    std::size_t _block_count;

    iterator begin() const noexcept
    {
        return {0};
    }
    iterator end() const noexcept
    {
        return {std::uint16_t(_block_count)};
    }

    std::size_t size() const noexcept
    {
        return _block_count;
    }
};

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

    std::size_t size() const noexcept
    {
        return std::size_t(_end - _begin);
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

    std::optional<block_idx> lexical_next_block(block_idx idx) const noexcept
    {
        auto value = std::size_t(idx);
        if (value == _blocks.size() - 1)
            return std::nullopt;
        else
            return block_idx(value + 1);
    }

    ir_block_range blocks() const noexcept
    {
        return {_blocks.size()};
    }

    ir_inst_range instructions() const noexcept
    {
        return {_instructions.begin(), _instructions.end()};
    }

    const ir_inst& instruction(std::size_t idx) const noexcept
    {
        return _instructions[idx];
    }

    std::size_t index_of(const ir_inst& inst) const noexcept
    {
        return std::size_t(&inst - _instructions.data());
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

