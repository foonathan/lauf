// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/ir/irdump.hpp>

namespace
{
std::string format(lauf::param_idx idx)
{
    return std::to_string(std::size_t(idx));
}
std::string format(lauf::register_idx idx)
{
    return "%" + std::to_string(std::size_t(idx));
}
std::string format(lauf::block_idx idx)
{
    return "%bb_" + std::to_string(std::size_t(idx));
}
} // namespace

std::string lauf::irdump(const ir_function& fn)
{
    std::string result;

    auto cur_indent   = std::size_t(0);
    auto print_indent = [&](char first = ' ') {
        if (first == ' ')
            result.append(cur_indent * 4, ' ');
        else
        {
            result += first;
            result.append(cur_indent * 4 - 1, ' ');
        }
    };

    auto print_result = [&](const ir_inst& inst) {
        result += " => ";
        if (inst.tag.uses == 0)
            result += "_";
        else
            result += format(register_idx(&inst - fn.instructions().begin()));
    };
    auto print_call_results = [&](const ir_inst*& iter) {
        result += " => ";

        // +1 because iter points to the call instruction (after skipping arguments).
        for (auto first = true; iter[1].tag.op == ir_op::call_result; ++iter)
        {
            if (first)
                first = false;
            else
                result += ", ";

            if (iter[1].tag.uses == 0)
                result += "_";
            else
                result += format(register_idx(&iter[1] - fn.instructions().begin()));
        }
    };

    auto print_argument = [&](ir_inst inst) {
        if (inst.argument.is_constant)
            result += std::to_string(inst.const_.value.as_uint);
        else
            result += format(inst.argument.register_idx);
    };
    auto print_arguments = [&](const ir_inst* iter, std::size_t arg_count) {
        result += "(";
        for (auto i = 0u; i != arg_count; ++i)
        {
            if (i > 0)
                result += ", ";
            // +1 because iter points to the parent instruction
            print_argument(iter[arg_count - i]);
        }
        result += ")";
        return iter + arg_count;
    };

    for (auto i = 0u; i != fn.block_count(); ++i)
    {
        //=== block heading ===//
        print_indent();
        result += "block ";
        result += format(block_idx(i));
        result += '\n';

        print_indent();
        result += "{\n";
        ++cur_indent;

        //=== instructions ===//
        auto range = fn.block(block_idx(i));
        for (auto iter = range.begin(); iter != range.end(); ++iter)
        {
            auto& inst = *iter;
            if (inst.tag.op == ir_op::const_ && inst.tag.uses == 0)
                continue;
            else if (inst.tag.uses == 0)
                print_indent('#');
            else
                print_indent();

            switch (inst.tag.op)
            {
            case ir_op::return_:
                result += "return";
                iter = print_arguments(iter, inst.jump.argument_count);
                break;
            case ir_op::jump:
                result += "jump ";
                result += format(inst.jump.dest);
                iter = print_arguments(iter, inst.jump.argument_count);
                break;
            case ir_op::branch:
                result += "branch.";
                result += to_string(inst.branch.cc);
                result += " ";
                result += format(inst.branch.reg);
                result += " if ";
                result += format(inst.branch.if_true);
                print_arguments(iter, inst.branch.argument_count);
                result += " else ";
                result += format(inst.branch.if_false);
                iter = print_arguments(iter, inst.branch.argument_count);
                break;

            case ir_op::param:
                result += "param ";
                result += format(inst.param.index);
                print_result(inst);
                break;

            case ir_op::const_:
                result += std::to_string(inst.const_.value.as_uint);
                print_result(inst);
                break;

            case ir_op::call_builtin:
                result += "$<";
                result += std::to_string(reinterpret_cast<std::uintptr_t>(inst.call_builtin.fn));
                result += ">";
                iter = print_arguments(iter, inst.call_builtin.signature.input_count);
                print_call_results(iter);
                break;
            case ir_op::call:
                result += "call @";
                result += lauf_function_get_name(inst.call.fn);
                iter = print_arguments(iter, inst.call.signature.input_count);
                print_call_results(iter);
                break;

            case ir_op::load_value:
                result += "load_value ";
                result += std::to_string(inst.load_value.local_addr);
                print_result(inst);
                break;
            case ir_op::store_value:
                result += "store_value ";
                result += std::to_string(inst.store_value.local_addr);
                result += " ";
                result += format(inst.store_value.register_idx);
                break;

            case ir_op::argument:
            case ir_op::call_result:
                // Should be handled by owning instruction.
                assert(false);
                break;
            }
            result += ";\n";
        }

        //=== block suffix ===//
        --cur_indent;
        print_indent();
        result += "}\n";
    }

    return result;
}

