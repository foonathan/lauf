// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/ir/cfg.hpp>

#include <lauf/impl/module.hpp>
#include <optional>

using namespace lauf;

namespace
{
std::optional<control_flow_graph::basic_block> finish_bb(lauf_function fn, lauf_vm_instruction* ip,
                                                         lauf_vm_instruction* cur_block_start)
{
    switch (ip->tag.op)
    {
    case bc_op::exit:
    case bc_op::return_:
    case bc_op::return_no_alloc:
    case bc_op::panic:
        // Terminate current basic block with an exit.
        // Include the terminating instruction here.
        return control_flow_graph::basic_block(cur_block_start, ip + 1,
                                               control_flow_graph::terminator());

    case bc_op::label: {
        auto dest_idx = std::size_t((ip + 1) - fn->bytecode());

        // Terminate current block with a jump to the next instruction (fallthrough).
        // Do not include the terminating instruction.
        return control_flow_graph::basic_block(cur_block_start, ip,
                                               control_flow_graph::terminator(dest_idx));
    }

    case bc_op::jump: {
        auto dest_idx = std::size_t((ip + ip->jump.offset) - fn->bytecode());

        // Terminate current basic block with a jump.
        // Do not include the jump instruction.
        // Store instruction index instead of basic block index.
        return control_flow_graph::basic_block(cur_block_start, ip,
                                               control_flow_graph::terminator(dest_idx));
    }

    case bc_op::jump_if:
    case bc_op::jump_ifz:
    case bc_op::jump_ifge: {
        auto if_true_idx  = std::size_t((ip + ip->jump_if.offset + 1) - fn->bytecode());
        auto if_false_idx = std::size_t((ip + 1) - fn->bytecode());

        // Terminate current basic block with a branch.
        // Do not include the jump instruction.
        // Store instruction index instead of basic block index.
        return control_flow_graph::basic_block(cur_block_start, ip,
                                               control_flow_graph::terminator(ip->jump_if.cc,
                                                                              if_true_idx,
                                                                              if_false_idx));
    }

    default:
        // Not a control flow instruction, nothing needs to be done.
        return std::nullopt;
    }
}
} // namespace

control_flow_graph lauf::build_cfg(stack_allocator& alloc, lauf_function fn)
{
    control_flow_graph result(alloc);

    // Maps the index of a bytecode instruction to the basic block starting at that address.
    temporary_array<std::size_t> ip_to_bb(alloc, fn->instruction_count);
    ip_to_bb.resize(alloc, fn->instruction_count, std::size_t(-1));

    // First build all basic blocks.
    auto cur_block_start = fn->bytecode();
    auto cur_block_idx   = std::size_t(0);
    for (auto ip = fn->bytecode(); ip != fn->bytecode() + fn->instruction_count; ++ip)
    {
        if (auto bb = finish_bb(fn, ip, cur_block_start))
        {
            result._bbs.push_back(alloc, *bb);
            ip_to_bb[cur_block_start - fn->bytecode()] = cur_block_idx;

            cur_block_start = ip + 1;
            ++cur_block_idx;
        }
    }

    // Next replace the instruction indices by basic block indices.
    auto patch_terminator_idx = [&](std::size_t& next) {
        auto bb = ip_to_bb[next];
        assert(bb != std::size_t(-1)); // missing label instruction
        next = bb;
    };
    for (auto& bb : result._bbs)
    {
        switch (bb.terminator())
        {
        case control_flow_graph::terminator::exit:
            // Nothing to replace.
            break;
        case control_flow_graph::terminator::jump:
            patch_terminator_idx(bb._term._first);
            break;
        case control_flow_graph::terminator::branch:
            patch_terminator_idx(bb._term._first);
            patch_terminator_idx(bb._term._second);
            break;
        }
    }

    return result;
}

