// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/ir/irgen.hpp>

#include <algorithm>
#include <lauf/impl/module.hpp>

using namespace lauf;

namespace
{
class linker
{
public:
    explicit linker(stack_allocator& alloc, lauf_function fn)
    : _fn(fn), _map(alloc, fn->instruction_count)
    {
        _map.resize(alloc, fn->instruction_count, block_idx(-1));
    }

    void block_start(lauf_vm_instruction* ip, block_idx idx)
    {
        auto ip_idx  = std::size_t(ip - _fn->bytecode());
        _map[ip_idx] = idx;
    }

    block_idx early_lookup(lauf_vm_instruction* dest)
    {
        auto dest_idx = std::size_t(dest - _fn->bytecode());
        return block_idx(dest_idx);
    }

    block_idx link(block_idx dest_idx) const
    {
        auto result = _map[static_cast<std::size_t>(dest_idx)];
        assert(result != block_idx(-1));
        return result;
    }

private:
    lauf_function _fn;
    // Maps the index of a bytecode instruction to the basic block starting there.
    temporary_array<block_idx> _map;
};

class value_stack
{
public:
    explicit value_stack(stack_allocator& alloc, lauf_function fn)
    : _stack(alloc, fn->max_vstack_size)
    {}

    bool empty() const
    {
        return _stack.empty();
    }
    std::size_t size() const
    {
        return _stack.size();
    }

    void push(register_idx reg)
    {
        _stack.push_back(reg);
    }

    register_idx pop()
    {
        auto reg = _stack.back();
        _stack.pop_back();
        return reg;
    }

    ir_inst pop_argument(temporary_array<ir_inst>& inst)
    {
        auto reg = pop();
        auto idx = std::size_t(reg);
        if (inst[idx].tag.op == ir_op::const_)
        {
            // It is a constant, inline and decrement use count.
            --inst[idx].const_.uses;
            return LAUF_IR_INSTRUCTION(argument, inst[idx].const_.value);
        }
        else
        {
            return LAUF_IR_INSTRUCTION(argument, reg);
        }
    }

    void drop(temporary_array<ir_inst>& inst, std::size_t n)
    {
        for (auto idx = _stack.size() - n - 1; idx != _stack.size(); ++idx)
        {
            auto reg = _stack[idx];
            --inst[std::size_t(reg)].tag.uses;
        }
        _stack.resize(_stack.size() - n);
    }

    void pick(temporary_array<ir_inst>& inst, std::size_t idx)
    {
        auto reg = _stack[_stack.size() - idx - 1];
        ++inst[std::size_t(reg)].tag.uses;
        _stack.push_back(reg);
    }

    void roll(std::size_t idx)
    {
        auto iter = std::prev(_stack.end(), std::ptrdiff_t(idx) + 1);
        std::rotate(iter, iter + 1, _stack.end());
    }

private:
    temporary_array<register_idx> _stack;
};
} // namespace

ir_function lauf::irgen(stack_allocator& alloc, lauf_function fn)
{
    linker      l(alloc, fn);
    value_stack vstack(alloc, fn);
    ir_function result(alloc);

    auto add_inst = [&](ir_inst inst) {
        result._instructions.push_back(alloc, inst);
        return register_idx(result._instructions.size() - 1);
    };

    auto cur_block_begin = std::size_t(0);
    auto start_block     = [&](lauf_vm_instruction* ip, std::size_t arg_count) {
        l.block_start(ip, block_idx(result._blocks.size()));
        cur_block_begin = result._instructions.size();

        // Create the parameter instructions for the new block.
        for (auto i = 0u; i != arg_count; ++i)
        {
            // The last parameter is at the top of the stack.
            auto reg = add_inst(LAUF_IR_INSTRUCTION(param, param_idx(arg_count - i - 1)));
            vstack.push(reg);
        }
    };
    auto terminate_block = [&]() {
        if (cur_block_begin == result._instructions.size())
            return;

        // Everything currently on the stack is an argument for the block.
        // (The bytecode builder ensures that the counts are consistent for all basic blocks).
        auto arg_count = 0u;
        while (!vstack.empty())
        {
            auto arg = vstack.pop_argument(result._instructions);
            add_inst(arg);
            ++arg_count;
        }

        // Add the new block.
        auto cur_block_end = result._instructions.size();
        result._blocks.push_back(alloc, {cur_block_begin, cur_block_end});
        cur_block_begin = cur_block_end;
    };

    auto handle_call = [&](lauf_signature sig) {
        for (auto i = 0u; i != sig.input_count; ++i)
        {
            auto arg = vstack.pop_argument(result._instructions);
            add_inst(arg);
        }

        for (auto i = 0u; i != sig.output_count; ++i)
        {
            auto reg = add_inst(LAUF_IR_INSTRUCTION(call_result));
            vstack.push(reg);
        }
    };

    start_block(fn->bytecode(), fn->input_count);
    for (auto ip = fn->bytecode(); ip != fn->bytecode() + fn->instruction_count; ++ip)
    {
        switch (ip->tag.op)
        {
        case bc_op::nop:
            break;

        case bc_op::label:
            terminate_block();
            start_block(ip + 1, ip->label.literal);
            break;

        case bc_op::jump: {
            auto dest = l.early_lookup(ip + ip->jump.offset);
            add_inst(LAUF_IR_INSTRUCTION(jump, std::uint8_t(vstack.size()), dest));

            terminate_block();
            break;
        }
        case bc_op::jump_if:
        case bc_op::jump_ifz:
        case bc_op::jump_ifge: {
            auto reg = vstack.pop();

            auto arg_count = std::uint8_t(vstack.size());
            auto if_true   = l.early_lookup(ip + ip->jump_if.offset + 1);
            auto if_false  = l.early_lookup(ip + 1);
            add_inst(LAUF_IR_INSTRUCTION(branch, arg_count, //
                                         reg, ip->jump_if.cc, if_true, if_false));

            terminate_block();
            start_block(ip + 1, arg_count);
            break;
        }

        case bc_op::return_:
        case bc_op::return_no_alloc:
            add_inst(LAUF_IR_INSTRUCTION(return_, fn->output_count));
            terminate_block();
            break;

        case bc_op::call: {
            auto callee = fn->mod->function_begin()[size_t(ip->call.function_idx)];
            auto sig    = lauf_signature{callee->input_count, callee->output_count};
            add_inst(LAUF_IR_INSTRUCTION(call, sig, callee));
            handle_call(sig);
            break;
        }
        case bc_op::add_local_allocations:
            // Needs to be handled by the call instruction as well.
            break;
        case bc_op::call_builtin: {
            auto base_addr = reinterpret_cast<unsigned char*>(&lauf_builtin_finish);
            auto addr      = base_addr + ip->call_builtin.address * std::ptrdiff_t(16);
            auto callee    = reinterpret_cast<lauf_builtin_function*>(addr);

            auto sig = lauf_signature{std::uint8_t(ip->call_builtin.input_count),
                                      std::uint8_t(ip->call_builtin.output_count)};

            add_inst(LAUF_IR_INSTRUCTION(call_builtin, sig, callee));
            handle_call(sig);
            break;
        }
        case bc_op::call_builtin_long: {
            auto addr
                = fn->mod->literal_data()[size_t(ip->call_builtin_long.address)].as_native_ptr;
            auto callee = (lauf_builtin_function*)addr;

            auto sig = lauf_signature{std::uint8_t(ip->call_builtin_long.input_count),
                                      std::uint8_t(ip->call_builtin_long.output_count)};

            add_inst(LAUF_IR_INSTRUCTION(call_builtin, sig, callee));
            handle_call(sig);
            break;
        }

        case bc_op::push: {
            auto value = fn->mod->literal_data()[size_t(ip->push.literal_idx)];
            auto reg   = add_inst(LAUF_IR_INSTRUCTION(const_, value));
            vstack.push(reg);
            break;
        }
        case bc_op::push_zero: {
            auto value = lauf_value{};
            auto reg   = add_inst(LAUF_IR_INSTRUCTION(const_, value));
            vstack.push(reg);
            break;
        }
        case bc_op::push_small_zext: {
            auto value = lauf_value{.as_uint = ip->push_small_zext.literal};
            auto reg   = add_inst(LAUF_IR_INSTRUCTION(const_, value));
            vstack.push(reg);
            break;
        }
        case bc_op::push_small_neg: {
            auto value = lauf_value{.as_sint = -std::int64_t(ip->push_small_neg.literal)};
            auto reg   = add_inst(LAUF_IR_INSTRUCTION(const_, value));
            vstack.push(reg);
            break;
        }
        case bc_op::push_addr: {
            auto value = lauf_value{.as_address = lauf_value_address{ip->push_addr.literal, 0, 0}};
            auto reg   = add_inst(LAUF_IR_INSTRUCTION(const_, value));
            vstack.push(reg);
            break;
        }

        case bc_op::pop:
            vstack.drop(result._instructions, ip->pop.literal);
            break;
        case bc_op::pick:
        case bc_op::dup:
            vstack.pick(result._instructions, ip->pick.literal);
            break;
        case bc_op::roll:
        case bc_op::swap:
            vstack.roll(ip->roll.literal);
            break;

        case bc_op::load_value: {
            auto reg = add_inst(LAUF_IR_INSTRUCTION(load_value, ip->load_value.literal));
            vstack.push(reg);
            break;
        }
        case bc_op::store_value: {
            auto reg = vstack.pop();
            add_inst(LAUF_IR_INSTRUCTION(store_value, reg, ip->load_value.literal));
            break;
        }
        case bc_op::save_value: {
            vstack.pick(result._instructions, 0);
            auto reg = vstack.pop();
            add_inst(LAUF_IR_INSTRUCTION(store_value, reg, ip->load_value.literal));
            break;
        }

        case bc_op::push_local_addr:
        case bc_op::array_element_addr:
        case bc_op::aggregate_member_addr:
        case bc_op::select:
        case bc_op::select2:
        case bc_op::select_if:
        case bc_op::load_field:
        case bc_op::store_field:
        case bc_op::load_array_value:
        case bc_op::store_array_value:
        case bc_op::panic:
        case bc_op::panic_if:

        case bc_op::exit:
        case bc_op::_count:
            // Those should never show up here.
            assert(false);
            break;
        }
    }

    for (auto& inst : result._instructions)
        if (inst.tag.op == ir_op::jump)
        {
            inst.jump.dest = l.link(inst.jump.dest);
        }
        else if (inst.tag.op == ir_op::branch)
        {
            inst.branch.if_true  = l.link(inst.branch.if_true);
            inst.branch.if_false = l.link(inst.branch.if_false);
        }

    return result;
}

