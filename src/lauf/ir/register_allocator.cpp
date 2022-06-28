// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/ir/register_allocator.hpp>

#include <algorithm>
#include <optional>

using namespace lauf;

namespace
{
std::optional<register_idx> result_register(const ir_function& fn, const ir_inst& inst)
{
    switch (inst.tag.op)
    {
    case ir_op::param:
    case ir_op::const_:
    case ir_op::call_result:
    case ir_op::load_value:
        return register_idx(fn.index_of(inst));

    case ir_op::return_:
    case ir_op::jump:
    case ir_op::branch:
    case ir_op::call_builtin:
    case ir_op::call:
    case ir_op::argument:
    case ir_op::store_value:
        return std::nullopt;
    }
}

class register_set
{
public:
    explicit register_set(stack_allocator& alloc, std::uint8_t reg_count) : _regs(alloc, reg_count)
    {}

    void reset(std::uint8_t reg_count)
    {
        _regs.resize(0);

        // We iterate in reverse to use high values first.
        for (auto i = reg_count; i > 0; --i)
            _regs.push_back(i - 1);
    }

    void insert(std::uint8_t reg)
    {
        auto iter = std::find(_regs.begin(), _regs.end(), reg);
        assert(iter == _regs.end());
        _regs.push_back(reg);
    }

    std::optional<std::uint8_t> pop()
    {
        if (_regs.empty())
            return std::nullopt;

        auto reg = _regs.back();
        _regs.pop_back();
        return reg;
    }

private:
    temporary_array<std::uint8_t> _regs;
};
} // namespace

namespace
{
//=== step 1 ===//
// We first assign each register to either a temporary/persistent register ignoring any actual
// counts. A register is assigned to a temporary if its value does not need to be used after a
// function call.
void classify_temporary_persistent(register_assignments& result, const ir_function& fn)
{
    // This happens on a per-basic block level as control flow is important.
    for (auto bb : fn.blocks())
    {
        std::optional<std::size_t> last_call;
        for (auto& inst : fn.block(bb))
        {
            if (auto reg = result_register(fn, inst))
            {
                // We optimistically assume it's a temporary.
                // This is true for most registers that haven't been duplicated due to the stack
                // based nature.
                result.assign(*reg, {register_assignment::temporary_reg, 0});
            }

            // Downgrade registers to persistent if necessary.
            // It's necessary if there was a call between the definition and use.
            auto downgrade = [&](const ir_inst& use, register_idx reg) {
                if (!last_call)
                    return;

                if (std::size_t(reg) < *last_call && *last_call < fn.index_of(use))
                    result.assign(reg, {register_assignment::persistent_reg, 0});
            };
            switch (inst.tag.op)
            {
                // For calls, we update the index.
                // We add the number of arguments to it, as registers used there are still okay.
            case ir_op::call_builtin:
                last_call = fn.index_of(inst) + inst.call_builtin.signature.input_count;
                break;
            case ir_op::call:
                last_call = fn.index_of(inst) + inst.call.signature.input_count;
                break;

                // Instructions that read registers need to downgrade it if necessary.
            case ir_op::branch:
                downgrade(inst, inst.branch.reg);
                break;
            case ir_op::argument:
                if (!inst.argument.is_constant)
                    downgrade(inst, inst.argument.register_idx);
                break;
            case ir_op::store_value:
                downgrade(inst, inst.store_value.register_idx);
                break;

                // Other instructions don't affect anything.
            case ir_op::return_:
            case ir_op::jump:
            case ir_op::param:
            case ir_op::const_:
            case ir_op::call_result:
            case ir_op::load_value:
                break;
            }
        }
    }
}

//=== step 2 ===//
// We promote temporary registers to argument registers, if they're used as arguments.
// This is always safe as there is no intermediate call between them by definition.
// Also a temporary register can only be used as an argument at most once,
// so we can just look at the argument instructions.
// They're also the only ones that can be promoted, as persistent ones have intermediate calls
// which might clobber them.
void promote_to_argument(register_assignments& result, const machine_register_file& rf,
                         const ir_function& fn)
{
    auto arg_index = std::uint16_t(0);
    // We can directly iterate over all instructions as we do not need control flow information.
    for (auto& inst : fn.instructions())
    {
        switch (inst.tag.op)
        {
        case ir_op::return_:
            arg_index = inst.return_.argument_count;
            break;
        case ir_op::jump:
            arg_index = inst.jump.argument_count;
            break;
        case ir_op::branch:
            arg_index = inst.branch.argument_count;
            break;
        case ir_op::call_builtin:
            arg_index = inst.call_builtin.signature.input_count;
            break;
        case ir_op::call:
            arg_index = inst.call.signature.input_count;
            break;

        case ir_op::argument: {
            --arg_index;
            if (inst.argument.is_constant || arg_index >= rf.argument_count)
                break;

            auto reg = inst.argument.register_idx;
            if (result[reg].kind != register_assignment::temporary_reg)
                // Not a temporary register, can't promote.
                break;

            if (auto reg_inst = fn.instruction(std::size_t(reg));
                reg_inst.tag.op == ir_op::param && std::uint16_t(reg_inst.param.index) != arg_index)
                // The instruction is a parameter that currently resides in a different argument.
                // This means that we need to shuffle parameters around, which requires temporaries
                // for the swap.
                //
                // OPTIMIZE: we don't need to put everything into a temporary, some can be
                // moved directly.
                break;

            result.assign(reg, {register_assignment::argument_reg, arg_index});
            break;
        }

        case ir_op::param:
        case ir_op::const_:
        case ir_op::call_result:
        case ir_op::store_value:
        case ir_op::load_value:
            break;
        }
    }
}

//=== step 3 ===//
// At this point, argument registers are taken care of and properly assigned.
// We know need to do register allocation for temporary and persistent registers.
void allocate_temporary_persistent(register_assignments& result, stack_allocator& alloc,
                                   const machine_register_file& rf, const ir_function& fn)
{
    auto         marker = alloc.top();
    register_set temporary_regs(alloc, rf.temporary_count);
    register_set persistent_regs(alloc, rf.persistent_count);

    temporary_array<std::size_t> remaining_uses(alloc, fn.instructions().size());
    remaining_uses.resize(alloc, fn.instructions().size());

    for (auto bb : fn.blocks())
    {
        temporary_regs.reset(rf.temporary_count);
        persistent_regs.reset(rf.persistent_count);
        for (auto& inst : fn.block(bb))
        {
            // Allocate a new register for results.
            if (auto vreg = result_register(fn, inst); vreg && inst.tag.uses > 0)
            {
                if (auto assgn = result[*vreg]; assgn.kind == register_assignment::temporary_reg)
                {
                    if (auto reg = temporary_regs.pop())
                    {
                        result.assign(*vreg, {register_assignment::temporary_reg, *reg});
                    }
                    else
                    {
                        reg = persistent_regs.pop();
                        assert(reg); // TODO: spill
                        result.assign(*vreg, {register_assignment::persistent_reg, *reg});
                    }
                }
                else if (assgn.kind == register_assignment::persistent_reg)
                {
                    auto reg = persistent_regs.pop();
                    assert(reg); // TODO: spill
                    result.assign(*vreg, {register_assignment::persistent_reg, *reg});
                }
                remaining_uses[std::size_t(*vreg)] = inst.tag.uses;
            }

            // Free registers on their last use.
            auto free = [&](register_idx vreg) {
                if (--remaining_uses[std::size_t(vreg)] != 0)
                    return;

                if (auto assgn = result[vreg]; assgn.kind == register_assignment::temporary_reg)
                    temporary_regs.insert(assgn.index);
                else if (assgn.kind == register_assignment::persistent_reg)
                    persistent_regs.insert(assgn.index);
            };
            switch (inst.tag.op)
            {
                // Instructions that read registers need to downgrade it if necessary.
            case ir_op::branch:
                free(inst.branch.reg);
                break;
            case ir_op::argument:
                if (!inst.argument.is_constant)
                    free(inst.argument.register_idx);
                break;
            case ir_op::store_value:
                free(inst.store_value.register_idx);
                break;

                // Other instructions don't affect anything.
            case ir_op::return_:
            case ir_op::jump:
            case ir_op::param:
            case ir_op::const_:
            case ir_op::call_builtin:
            case ir_op::call:
            case ir_op::call_result:
            case ir_op::load_value:
                break;
            }
        }
    }

    alloc.unwind(marker);
}
} // namespace

register_assignments lauf::register_allocation(stack_allocator&             alloc,
                                               const machine_register_file& rf,
                                               const ir_function&           fn)
{
    register_assignments result(alloc, fn.instructions().size());
    classify_temporary_persistent(result, fn);
    promote_to_argument(result, rf, fn);
    allocate_temporary_persistent(result, alloc, rf, fn);
    return result;
}
