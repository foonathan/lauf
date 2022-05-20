// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_DETAIL_BYTECODE_BUILDER_HPP_INCLUDED
#define SRC_LAUF_DETAIL_BYTECODE_BUILDER_HPP_INCLUDED

#include <lauf/builder.h>
#include <lauf/detail/bytecode.hpp>
#include <lauf/impl/module.hpp>
#include <vector>

namespace lauf::_detail
{
class bytecode_builder
{
public:
    //=== label ===//
    lauf_label declare_label(std::size_t vstack_size)
    {
        auto idx = _labels.size();
        _labels.push_back({vstack_size, 0});
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

        // The next instruction is an entry point as we might jump to it.
        _is_entry_point = true;
    }

    //=== instruction ===//
    void location(lauf_debug_location location)
    {
        if (_locations.empty()
            || std::memcmp(&_locations.back().location, &location, sizeof(lauf_debug_location))
                   != 0)
            _locations.push_back({ptrdiff_t(_bytecode.size()), location});
    }

    void instruction(bc_inst inst)
    {
        _bytecode.push_back(inst);
        _is_entry_point = false;
    }
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
        if (_is_entry_point)
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

    void finish(bc_inst* dest)
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
                inst      = LAUF_VM_INSTRUCTION(jump_if, inst.jump_if.cc, dest - cur_offset);
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
        _labels.clear();
        _bytecode.clear();
        _locations.clear();
        _is_entry_point = true;
    }

private:
    struct label_decl
    {
        size_t vstack_size;
        // Set once the label is placed.
        ptrdiff_t bytecode_offset;
    };

    std::vector<bc_inst>                   _bytecode;
    std::vector<label_decl>                _labels;
    std::vector<debug_location_map::entry> _locations;

    // Set to indicate that the next instruction is going to be a potential entry point for a basic
    // block.
    bool _is_entry_point = true;
};
} // namespace lauf::_detail

#endif // SRC_LAUF_DETAIL_BYTECODE_BUILDER_HPP_INCLUDED

