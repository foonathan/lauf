// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_BYTECODE_BUILDER_HPP_INCLUDED
#define SRC_LAUF_BYTECODE_BUILDER_HPP_INCLUDED

#include <lauf/builder.h>
#include <lauf/bytecode.hpp>
#include <lauf/impl/module.hpp>
#include <lauf/support/temporary_array.hpp>
#include <vector>

namespace lauf
{
class bytecode_builder
{
public:
    explicit bytecode_builder(stack_allocator& alloc) : _alloc(&alloc) {}

    //=== label ===//
    lauf_label declare_label(std::size_t vstack_size)
    {
        auto idx = _labels.size();
        _labels.push_back(*_alloc, {vstack_size, 0});
        return {idx};
    }

    std::size_t get_label_stack_size(lauf_label l)
    {
        return _labels[l._idx].vstack_size;
    }

    void place_label(lauf_label l)
    {
        auto& decl           = _labels[l._idx];
        decl.bytecode_offset = ptrdiff_t(_bytecode.size());

        // We terminate the current basic block and add a new one.
        new_basic_block();
    }

    //=== instruction ===//
    void location(lauf_debug_location location)
    {
        if (location.line == 0u && location.column == 0u)
            return;

        if (_locations.empty()
            || std::memcmp(&_locations.back().location, &location, sizeof(lauf_debug_location))
                   != 0)
            _locations.push_back(*_alloc, {ptrdiff_t(_bytecode.size()), location});
    }

    void instruction(bc_inst inst)
    {
        _bytecode.push_back(*_alloc, inst);
        if (inst.tag.op == bc_op::return_ || inst.tag.op == bc_op::jump
            || inst.tag.op == bc_op::jump_if || inst.tag.op == bc_op::panic)
            new_basic_block();
    }

    void replace_entry_instruction(bc_inst inst)
    {
        _bytecode.front() = inst;
    }

    // We're assuming that this does not affect control flow.
    void replace_last_instruction(bc_inst inst)
    {
        _bytecode.back() = inst;
    }
    void replace_last_instruction(bc_op op)
    {
        _bytecode.back().tag.op = op;
    }

    //=== peephole ===//
    // Returns the instruction that necessarily needs to execute directly before the next
    // instruction.
    bc_inst get_cur_idom() const
    {
        if (_cur_basic_block_begin >= _bytecode.size())
            // If it's an entry point, there is no dominating instruction.
            return LAUF_VM_INSTRUCTION(nop);
        else
            return _bytecode.back();
    }

    // Returns whether the next instruction can be reached by fallthrough of the previous
    // instruction.
    bool can_fallthrough() const
    {
        if (_bytecode.empty())
            return false;

        auto op = _bytecode.back().tag.op;
        return op != bc_op::jump && op != bc_op::return_ && op != bc_op::panic;
    }

    //=== finish ===//
    std::size_t size() const
    {
        return _bytecode.size();
    }

    void finish(bc_inst* dest, bool has_local_allocations = true)
    {
        auto cur_offset = ptrdiff_t(0);
        for (auto inst : _bytecode)
        {
            if (inst.tag.op == bc_op::jump)
            {
                auto dest = _labels[inst.jump.offset].bytecode_offset;
                inst      = LAUF_VM_INSTRUCTION(jump, dest - cur_offset);
            }
            else if (inst.tag.op == bc_op::jump_if)
            {
                auto dest = _labels[inst.jump_if.offset].bytecode_offset;
                // The +1 is there because it always increments by one unconditionally.
                inst = LAUF_VM_INSTRUCTION(jump_if, inst.jump_if.cc, dest - (cur_offset + 1));

                if (inst.jump_if.cc == condition_code::is_zero)
                    inst.jump_if.op = bc_op::jump_ifz;
                else if (inst.jump_if.cc == condition_code::cmp_ge)
                    inst.jump_if.op = bc_op::jump_ifge;
            }
            else if (inst.tag.op == bc_op::return_)
            {
                if (!has_local_allocations)
                    inst = LAUF_VM_INSTRUCTION(return_no_alloc);
            }

            dest[cur_offset] = inst;
            ++cur_offset;
        }
    }

    debug_location_map debug_locations()
    {
        return debug_location_map(_locations.data(), _locations.size());
    }

    void reset()
    {
        _labels.clear_and_reserve(*_alloc, 32);
        _bytecode.clear_and_reserve(*_alloc, 512);
        _locations.clear_and_reserve(*_alloc, 512);
        _cur_basic_block_begin = 0;

        // We keep one nop instruction at the beginning, so we can properly patch it.
        instruction(LAUF_VM_INSTRUCTION(nop));
    }

private:
    struct label_decl
    {
        size_t vstack_size;
        // Set once the label is placed.
        ptrdiff_t bytecode_offset;
    };

    void new_basic_block()
    {
        _cur_basic_block_begin = _bytecode.size();
    }

    stack_allocator*                           _alloc;
    temporary_array<bc_inst>                   _bytecode;
    temporary_array<label_decl>                _labels;
    temporary_array<debug_location_map::entry> _locations;
    std::size_t                                _cur_basic_block_begin = 0;
};
} // namespace lauf

#endif // SRC_LAUF_BYTECODE_BUILDER_HPP_INCLUDED

