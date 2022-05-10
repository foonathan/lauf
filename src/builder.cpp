// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/builder.h>

#include <cassert>
#include <deque>
#include <lauf/builtin.h>
#include <lauf/detail/bytecode.hpp>
#include <lauf/detail/constant_pool.hpp>
#include <lauf/detail/stack_allocator.hpp>
#include <lauf/detail/stack_check.hpp>
#include <lauf/detail/verify.hpp>
#include <lauf/impl/module.hpp>
#include <vector>

using namespace lauf::_detail;

namespace
{
struct block_terminator
{
    enum kind
    {
        invalid,
        return_,
        jump,
        branch,
    } kind;
    union
    {
        lauf_block_builder dest;
        lauf_block_builder if_true;
    };
    lauf_block_builder if_false;
    lauf_condition     condition;

    block_terminator() : kind(invalid), dest{}, if_false{}, condition{} {}

    std::size_t max_instruction_count() const
    {
        switch (kind)
        {
        case invalid:
            return 0;
        case return_:
        case jump:
            return 1;
        case branch:
            return 2;
        }
    }
};
} // namespace

struct lauf_block_builder_impl
{
    lauf_function_builder       fn;
    block_terminator            terminator;
    lauf_signature              sig;
    stack_checker               vstack;
    std::vector<bc_instruction> bytecode;
    // Set while finishing the function, to resolve jumps to the beginning of the block.
    ptrdiff_t start_offset;

    lauf_block_builder_impl(lauf_function_builder parent, lauf_signature sig)
    : fn(parent), sig(sig), start_offset(0)
    {
        vstack.push(sig.input_count);
    }

    lauf_block_builder_impl(const lauf_block_builder_impl&) = delete;
    lauf_block_builder_impl& operator=(const lauf_block_builder_impl&) = delete;
};

struct lauf_function_builder_impl
{
    lauf_module_builder                 mod;
    const char*                         name;
    lauf_function                       fn;
    std::deque<lauf_block_builder_impl> blocks;
    stack_allocator_offset              locals;
    lauf_signature                      sig;
    bc_function_idx                     index;

    lauf_function_builder_impl(size_t index, lauf_module_builder mod, const char* name,
                               lauf_signature sig)
    : mod(mod), name(name), fn(nullptr), sig(sig), index(bc_function_idx(index))
    {}

    lauf_function_builder_impl(const lauf_function_builder_impl&) = delete;
    lauf_function_builder_impl& operator=(const lauf_function_builder_impl&) = delete;

    lauf_block_builder entry_block()
    {
        assert(!blocks.empty());
        return &blocks[0];
    }
};

struct lauf_module_builder_impl
{
    const char*                            name;
    constant_pool                          constants;
    std::deque<lauf_function_builder_impl> functions;
};

//=== module builder ===//
lauf_module_builder lauf_build_module(const char* name)
{
    return new lauf_module_builder_impl{name};
}

lauf_module lauf_finish_module(lauf_module_builder b)
{
    auto result            = lauf_impl_allocate_module(b->functions.size(), b->constants.size());
    result->function_count = b->functions.size();

    for (auto idx = std::size_t(0); idx != b->functions.size(); ++idx)
        result->function_begin()[idx] = b->functions[idx].fn;

    if (b->constants.size() != 0)
        std::memcpy(result->constant_data(), b->constants.data(),
                    b->constants.size() * sizeof(lauf_value));

    delete b;
    return result;
}

//=== function builder ===//
lauf_function_builder lauf_build_function(lauf_module_builder b, const char* name,
                                          lauf_signature sig)
{
    return &b->functions.emplace_back(b->functions.size(), b, name, sig);
}

lauf_function lauf_finish_function(lauf_function_builder b)
{
    // We get an upper bound of the bytecode size by assuming all blocks are reachable,
    // and have to generate the worst case amount of jump instructions.
    auto bytecode_size_estimate = [&] {
        auto result = std::size_t(0);
        for (auto& block : b->blocks)
        {
            result += block.bytecode.size();
            result += block.terminator.max_instruction_count();
        }
        return result;
    }();

    // Allocate the function and set its header.
    auto result              = lauf_impl_allocate_function(bytecode_size_estimate);
    result->name             = b->name;
    result->local_stack_size = static_cast<uint16_t>(b->locals.size());
    result->max_vstack_size  = [&] {
        auto result = std::size_t(0);
        for (auto& block : b->blocks)
            if (block.vstack.max_stack_size() > result)
                result = block.vstack.max_stack_size();
        return result;
    }();
    result->input_count  = b->sig.input_count;
    result->output_count = b->sig.output_count;

    // Copy bytecode and generate jump instructions.
    // This is completely trivial without any optimizations (for now?).
    auto bytecode = result->bytecode();
    for (auto& block : b->blocks)
    {
        block.start_offset = bytecode - result->bytecode();
        if (!block.bytecode.empty())
            std::memcpy(bytecode, block.bytecode.data(),
                        block.bytecode.size() * sizeof(bc_instruction));
        // We reserve space for all necessary jump instructions.
        bytecode += block.bytecode.size() + block.terminator.max_instruction_count();
    }

    // Now we know the position of every block and can generate the jumps.
    for (auto& block : b->blocks)
    {
        auto position = block.start_offset + ptrdiff_t(block.bytecode.size());
        auto term     = block.terminator;
        switch (term.kind)
        {
        case block_terminator::invalid:
            assert(false);
            break;

        case block_terminator::return_:
            result->bytecode()[position] = LAUF_BC_INSTRUCTION(return_);
            break;

        case block_terminator::jump:
            result->bytecode()[position]
                = LAUF_BC_INSTRUCTION(jump, term.dest->start_offset - position);
            break;

        case block_terminator::branch: {
            auto cc = [&] {
                switch (term.condition)
                {
                case LAUF_IF_FALSE:
                    return condition_code::if_zero;
                case LAUF_IF_TRUE:
                    return condition_code::if_nonzero;
                case LAUF_CMP_EQ:
                    return condition_code::cmp_eq;
                case LAUF_CMP_NE:
                    return condition_code::cmp_ne;
                case LAUF_CMP_LT:
                    return condition_code::cmp_lt;
                case LAUF_CMP_LE:
                    return condition_code::cmp_le;
                case LAUF_CMP_GT:
                    return condition_code::cmp_gt;
                case LAUF_CMP_GE:
                    return condition_code::cmp_ge;
                }
            }();
            result->bytecode()[position]
                = LAUF_BC_INSTRUCTION(jump_if, cc, term.if_true->start_offset - position);
            result->bytecode()[position + 1]
                = LAUF_BC_INSTRUCTION(jump, term.if_false->start_offset - (position + 1));
            break;
        }
        }
    }

    b->fn = result;
    return result;
}

lauf_local_variable lauf_build_local_variable(lauf_function_builder b, lauf_layout layout)
{
    auto addr = b->locals.allocate(layout.size, layout.alignment);
    return {addr};
}

//=== block builder ===//
lauf_block_builder lauf_build_block(lauf_function_builder b, lauf_signature sig)
{
    if (b->blocks.empty())
        LAUF_VERIFY(sig.input_count == b->sig.input_count, "block",
                    "entry block starts with function arguments");
    return &b->blocks.emplace_back(b, sig);
}

void lauf_finish_block_return(lauf_block_builder b)
{
    b->terminator.kind = block_terminator::return_;

    auto output_count = b->fn->sig.output_count;
    LAUF_VERIFY(b->vstack.cur_stack_size() == output_count, "return",
                "missing or too many values for return");
    LAUF_VERIFY(b->sig.output_count == output_count, "return",
                "invalid signature for terminator block");
    b->vstack.drop(b->fn->sig.output_count);
}

void lauf_finish_block_jump(lauf_block_builder b, lauf_block_builder dest)
{
    b->terminator.kind = block_terminator::jump;
    b->terminator.dest = dest;

    LAUF_VERIFY(b->vstack.cur_stack_size() == b->sig.output_count, "jump",
                "invalid stack size on block exit");
    LAUF_VERIFY(b->sig.output_count == dest->sig.input_count, "jump",
                "cannot jump to block expecting a different stack size");
}

void lauf_finish_block_branch(lauf_block_builder b, lauf_condition condition,
                              lauf_block_builder if_true, lauf_block_builder if_false)
{
    b->terminator.kind      = block_terminator::branch;
    b->terminator.condition = condition;
    b->terminator.if_true   = if_true;
    b->terminator.if_false  = if_false;

    LAUF_VERIFY_RESULT(b->vstack.drop(), "branch", "missing value for condition");
    LAUF_VERIFY(b->vstack.cur_stack_size() == b->sig.output_count, "branch",
                "invalid stack size on block exit");
    LAUF_VERIFY(b->sig.output_count == if_true->sig.input_count
                    && b->sig.output_count == if_false->sig.input_count,
                "branch", "cannot jump to block expecting a different stack size");
}

//=== instructions ===//
void lauf_build_int(lauf_block_builder b, lauf_value_int value)
{
    if (value == 0)
    {
        b->bytecode.push_back(LAUF_BC_INSTRUCTION(push_zero));
    }
    else if (0 < value && value <= 0xFF'FFFF)
    {
        b->bytecode.push_back(LAUF_BC_INSTRUCTION(push_small_zext, uint32_t(value)));
    }
    else if (-0xFF'FFFF <= value && value < 0)
    {
        b->bytecode.push_back(LAUF_BC_INSTRUCTION(push_small_neg, uint32_t(-value)));
    }
    else
    {
        auto idx = b->fn->mod->constants.insert(value);
        b->bytecode.push_back(LAUF_BC_INSTRUCTION(push, idx));
    }

    b->vstack.push();
}

void lauf_build_local_addr(lauf_block_builder b, lauf_local_variable var)
{
    b->bytecode.push_back(LAUF_BC_INSTRUCTION(local_addr, var._addr));
    b->vstack.push();
}

void lauf_build_drop(lauf_block_builder b, size_t n)
{
    b->bytecode.push_back(LAUF_BC_INSTRUCTION(drop, n));
    LAUF_VERIFY_RESULT(b->vstack.drop(n), "drop", "not enough values to pop");
}

void lauf_build_pick(lauf_block_builder b, size_t n)
{
    if (n == 0)
        b->bytecode.push_back(LAUF_BC_INSTRUCTION(dup));
    else
        b->bytecode.push_back(LAUF_BC_INSTRUCTION(pick, n));
    LAUF_VERIFY(n < b->vstack.cur_stack_size(), "pick", "invalid stack index");
    b->vstack.push();
}

void lauf_build_roll(lauf_block_builder b, size_t n)
{
    if (n == 0)
    {} // noop
    else if (n == 1)
        b->bytecode.push_back(LAUF_BC_INSTRUCTION(swap));
    else
        b->bytecode.push_back(LAUF_BC_INSTRUCTION(roll, n));
    LAUF_VERIFY(n < b->vstack.cur_stack_size(), "roll", "invalid stack index");
}

void lauf_build_recurse(lauf_block_builder b)
{
    lauf_build_call(b, b->fn);
}

void lauf_build_call(lauf_block_builder b, lauf_function_builder fn)
{
    b->bytecode.push_back(LAUF_BC_INSTRUCTION(call, fn->index));

    LAUF_VERIFY_RESULT(b->vstack.drop(fn->sig.input_count), "call", "missing arguments for call");
    b->vstack.push(fn->sig.output_count);
}

void lauf_build_call_builtin(lauf_block_builder b, struct lauf_builtin fn)
{
    auto idx = b->fn->mod->constants.insert(reinterpret_cast<void*>(fn.impl));
    b->bytecode.push_back(LAUF_BC_INSTRUCTION(call_builtin, idx));

    LAUF_VERIFY_RESULT(b->vstack.drop(fn.signature.input_count), "call_builtin",
                       "missing arguments for call");
    b->vstack.push(fn.signature.output_count);
}

void lauf_build_array_element(lauf_block_builder b, lauf_type type)
{
    b->bytecode.push_back(LAUF_BC_INSTRUCTION(array_element, type->layout.size));

    LAUF_VERIFY_RESULT(b->vstack.drop(2), "array_element", "missing object address or index");
    b->vstack.push();
}

void lauf_build_load_field(lauf_block_builder b, lauf_type type, size_t field)
{
    LAUF_VERIFY(field < type->field_count, "store_field", "invalid field count for type");

    auto idx = b->fn->mod->constants.insert(type);
    b->bytecode.push_back(LAUF_BC_INSTRUCTION(load_field, field, idx));

    LAUF_VERIFY_RESULT(b->vstack.drop(), "load_field", "missing object address");
    b->vstack.push();
}

void lauf_build_store_field(lauf_block_builder b, lauf_type type, size_t field)
{
    LAUF_VERIFY(field < type->field_count, "store_field", "invalid field count for type");

    auto idx = b->fn->mod->constants.insert(type);
    b->bytecode.push_back(LAUF_BC_INSTRUCTION(store_field, field, idx));

    LAUF_VERIFY_RESULT(b->vstack.drop(2), "store_field", "missing object address or value");
}

void lauf_build_load_value(lauf_block_builder b, lauf_local_variable var)
{
    b->bytecode.push_back(LAUF_BC_INSTRUCTION(load_value, var._addr));
    b->vstack.push();
}

void lauf_build_store_value(lauf_block_builder b, lauf_local_variable var)
{
    b->bytecode.push_back(LAUF_BC_INSTRUCTION(store_value, var._addr));
    LAUF_VERIFY_RESULT(b->vstack.drop(), "store_value", "missing value");
}

