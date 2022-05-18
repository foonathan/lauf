// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/builder.h>

#include <cassert>
#include <deque>
#include <lauf/builtin.h>
#include <lauf/detail/bytecode.hpp>
#include <lauf/detail/literal_pool.hpp>
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
    lauf_function_builder            fn;
    lauf_signature                   sig;
    block_terminator                 terminator;
    stack_checker                    vstack;
    std::vector<lauf_vm_instruction> bytecode;
    std::vector<debug_location_map>  debug_locations;
    // Set while finishing the function, to resolve jumps to the beginning of the block.
    ptrdiff_t start_offset;

    lauf_block_builder_impl(lauf_function_builder parent, lauf_signature sig)
    : fn(parent), sig(sig), vstack(sig), start_offset(0)
    {}

    lauf_block_builder_impl(const lauf_block_builder_impl&) = delete;
    lauf_block_builder_impl& operator=(const lauf_block_builder_impl&) = delete;
};

struct lauf_function_builder_impl
{
    lauf_module_builder                 mod;
    const char*                         name;
    lauf_debug_location                 debug_location;
    lauf_function                       fn;
    std::deque<lauf_block_builder_impl> blocks;
    stack_allocator_offset              locals;
    lauf_signature                      sig;
    bc_function_idx                     index;

    lauf_function_builder_impl(size_t index, lauf_module_builder mod, const char* name,
                               lauf_signature sig)
    : mod(mod), name(name), debug_location{}, fn(nullptr), sig(sig), index(bc_function_idx(index))
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
    const char*                            path;
    literal_pool                           literals;
    std::deque<lauf_function_builder_impl> functions;
};

//=== module builder ===//
lauf_module_builder lauf_build_module(const char* name)
{
    return new lauf_module_builder_impl{name};
}

lauf_module lauf_finish_module(lauf_module_builder b)
{
    auto result            = lauf_impl_allocate_module(b->functions.size(), b->literals.size());
    result->function_count = b->functions.size();
    result->path           = b->path;

    for (auto idx = std::size_t(0); idx != b->functions.size(); ++idx)
    {
        result->function_begin()[idx]      = b->functions[idx].fn;
        result->function_begin()[idx]->mod = result;
    }

    if (b->literals.size() != 0)
        std::memcpy(result->literal_data(), b->literals.data(),
                    b->literals.size() * sizeof(lauf_value));

    delete b;
    return result;
}

void lauf_build_module_path(lauf_module_builder b, const char* path)
{
    b->path = path;
}

//=== function builder ===//
namespace
{
condition_code translate_condition(lauf_condition cond)
{
    switch (cond)
    {
    case LAUF_IS_FALSE:
        return condition_code::is_zero;
    case LAUF_IS_TRUE:
        return condition_code::is_nonzero;
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
}
} // namespace

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

    // The local stack frame needs to be aligned properly.
    // This means we can allocate it in the VM without worrying about alignment.
    b->locals.align_to(alignof(std::max_align_t));

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
                        block.bytecode.size() * sizeof(lauf_vm_instruction));
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
            if (!block.bytecode.empty() && result->bytecode()[position - 1].tag.op == bc_op::call)
            {
                // Turn the last call into a tail call.
                auto fn = result->bytecode()[position - 1].call.function_idx;
                result->bytecode()[position - 1] = LAUF_VM_INSTRUCTION(tail_call, fn);
            }
            else
            {
                result->bytecode()[position] = LAUF_VM_INSTRUCTION(return_);
            }
            break;

        case block_terminator::jump:
            result->bytecode()[position]
                = LAUF_VM_INSTRUCTION(jump, term.dest->start_offset - position);
            break;

        case block_terminator::branch: {
            auto cc = translate_condition(term.condition);
            result->bytecode()[position]
                = LAUF_VM_INSTRUCTION(jump_if, cc, term.if_true->start_offset - position);
            result->bytecode()[position + 1]
                = LAUF_VM_INSTRUCTION(jump, term.if_false->start_offset - (position + 1));
            break;
        }
        }
    }

    // Finally set the debug locations.
    {
        auto debug_location_size = [&] {
            auto result = std::size_t(2);
            for (auto& block : b->blocks)
                result += block.debug_locations.size();
            return result;
        }();
        result->debug_locations = new debug_location_map[debug_location_size];

        auto ptr = result->debug_locations;
        *ptr++   = debug_location_map{-1, b->debug_location};
        for (auto& block : b->blocks)
            for (auto loc : block.debug_locations)
            {
                loc.first_address += block.start_offset;
                *ptr++ = loc;
            }
        *ptr++ = debug_location_map{-1, {}};
    }

    b->fn = result;
    return result;
}

void lauf_build_function_debug_location(lauf_function_builder b, lauf_debug_location location)
{
    b->debug_location = location;
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

    b->vstack.finish_return("return", b->fn->sig);
}

void lauf_finish_block_jump(lauf_block_builder b, lauf_block_builder dest)
{
    b->terminator.kind = block_terminator::jump;
    b->terminator.dest = dest;

    b->vstack.finish_jump("jump", dest->sig);
}

void lauf_finish_block_branch(lauf_block_builder b, lauf_condition condition,
                              lauf_block_builder if_true, lauf_block_builder if_false)
{
    b->terminator.kind      = block_terminator::branch;
    b->terminator.condition = condition;
    b->terminator.if_true   = if_true;
    b->terminator.if_false  = if_false;

    b->vstack.pop("branch");
    b->vstack.finish_jump("branch", if_true->sig);
    b->vstack.finish_jump("branch", if_false->sig);
}

void lauf_build_debug_location(lauf_block_builder b, lauf_debug_location location)
{
    if (b->debug_locations.empty()
        || std::memcmp(&b->debug_locations.back().location, &location, sizeof(lauf_debug_location))
               != 0)
        b->debug_locations.push_back({ptrdiff_t(b->bytecode.size()), location});
}

//=== instructions ===//
void lauf_build_int(lauf_block_builder b, lauf_value_sint value)
{
    if (value == 0)
    {
        b->bytecode.push_back(LAUF_VM_INSTRUCTION(push_zero));
    }
    else if (0 < value && value <= 0xFF'FFFF)
    {
        b->bytecode.push_back(LAUF_VM_INSTRUCTION(push_small_zext, uint32_t(value)));
    }
    else if (-0xFF'FFFF <= value && value < 0)
    {
        b->bytecode.push_back(LAUF_VM_INSTRUCTION(push_small_neg, uint32_t(-value)));
    }
    else
    {
        auto idx = b->fn->mod->literals.insert(value);
        b->bytecode.push_back(LAUF_VM_INSTRUCTION(push, idx));
    }

    b->vstack.push("int");
}

void lauf_build_ptr(lauf_block_builder b, lauf_value_ptr ptr)
{
    auto idx = b->fn->mod->literals.insert(ptr);
    b->bytecode.push_back(LAUF_VM_INSTRUCTION(push, idx));

    b->vstack.push("ptr");
}

void lauf_build_local_addr(lauf_block_builder b, lauf_local_variable var)
{
    b->bytecode.push_back(LAUF_VM_INSTRUCTION(local_addr, var._addr));
    b->vstack.push("local_addr");
}

void lauf_build_drop(lauf_block_builder b, size_t n)
{
    b->bytecode.push_back(LAUF_VM_INSTRUCTION(drop, n));
    b->vstack.pop("drop", n);
}

void lauf_build_pick(lauf_block_builder b, size_t n)
{
    if (n == 0)
        b->bytecode.push_back(LAUF_VM_INSTRUCTION(dup));
    else
        b->bytecode.push_back(LAUF_VM_INSTRUCTION(pick, n));
    LAUF_VERIFY(n < b->vstack.cur_stack_size(), "pick", "invalid stack index");
    b->vstack.push("pick");
}

void lauf_build_roll(lauf_block_builder b, size_t n)
{
    if (n == 0)
    {} // noop
    else if (n == 1)
        b->bytecode.push_back(LAUF_VM_INSTRUCTION(swap));
    else
        b->bytecode.push_back(LAUF_VM_INSTRUCTION(roll, n));
    LAUF_VERIFY(n < b->vstack.cur_stack_size(), "roll", "invalid stack index");
}

void lauf_build_call(lauf_block_builder b, lauf_function_builder fn)
{
    b->bytecode.push_back(LAUF_VM_INSTRUCTION(call, fn->index));

    b->vstack.pop("call", fn->sig.input_count);
    b->vstack.push("call", fn->sig.output_count);
}

void lauf_build_call_builtin(lauf_block_builder b, struct lauf_builtin fn)
{
    auto idx          = b->fn->mod->literals.insert(reinterpret_cast<void*>(fn.impl));
    auto stack_change = int32_t(fn.signature.input_count) - int32_t(fn.signature.output_count);
    b->bytecode.push_back(LAUF_VM_INSTRUCTION(call_builtin, stack_change, idx));

    b->vstack.pop("call_builtin", fn.signature.input_count);
    b->vstack.push("call_builtin", fn.signature.output_count);
}

void lauf_build_array_element(lauf_block_builder b, lauf_type type)
{
    b->bytecode.push_back(LAUF_VM_INSTRUCTION(array_element, type->layout.size));

    b->vstack.pop("array_element", 2);
    b->vstack.push("array_element");
}

void lauf_build_load_field(lauf_block_builder b, lauf_type type, size_t field)
{
    LAUF_VERIFY(field < type->field_count, "store_field", "invalid field count for type");

    auto idx = b->fn->mod->literals.insert(type);
    // If the last instruction is a store of the same field, turn it into a save instead.
    if (!b->bytecode.empty() && b->bytecode.back() == LAUF_VM_INSTRUCTION(store_field, field, idx))
        b->bytecode.back().store_field.op = bc_op::save_field;
    else
        b->bytecode.push_back(LAUF_VM_INSTRUCTION(load_field, field, idx));

    b->vstack.pop("load_field");
    b->vstack.push("load_field");
}

void lauf_build_store_field(lauf_block_builder b, lauf_type type, size_t field)
{
    LAUF_VERIFY(field < type->field_count, "store_field", "invalid field count for type");

    auto idx = b->fn->mod->literals.insert(type);
    b->bytecode.push_back(LAUF_VM_INSTRUCTION(store_field, field, idx));

    b->vstack.pop("store_field", 2);
}

void lauf_build_load_value(lauf_block_builder b, lauf_local_variable var)
{
    // If the last instruction is a store of the same address, turn it into a save instead.
    if (!b->bytecode.empty() && b->bytecode.back() == LAUF_VM_INSTRUCTION(store_value, var._addr))
        b->bytecode.back().store_value.op = bc_op::save_value;
    else
        b->bytecode.push_back(LAUF_VM_INSTRUCTION(load_value, var._addr));
    b->vstack.push("load_value");
}

void lauf_build_store_value(lauf_block_builder b, lauf_local_variable var)
{
    b->bytecode.push_back(LAUF_VM_INSTRUCTION(store_value, var._addr));
    b->vstack.pop("store_value");
}

void lauf_build_panic(lauf_block_builder b)
{
    b->bytecode.push_back(LAUF_VM_INSTRUCTION(panic));
    b->vstack.pop("panic");
}

void lauf_build_panic_if(lauf_block_builder b, lauf_condition condition)
{
    b->bytecode.push_back(LAUF_VM_INSTRUCTION(panic_if, translate_condition(condition)));
    b->vstack.pop("panic_if", 2);
}

