// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/builder.h>

#include "detail/bytecode.hpp"
#include "detail/constant_pool.hpp"
#include "detail/stack_check.hpp"
#include "detail/verify.hpp"
#include "impl/module.hpp"
#include <cassert>
#include <lauf/builtin.h>
#include <vector>

using namespace lauf::_detail;

namespace
{
struct function_decl
{
    const char*    name;
    lauf_signature sig;
    lauf_function  fn;
};

struct call_patch
{
    std::size_t caller;
    std::size_t bc_index;
    std::size_t callee;
};
} // namespace

struct lauf_builder_impl
{
    const char*                mod_name;
    constant_pool              constants;
    std::vector<function_decl> functions;
    std::vector<call_patch>    call_patches;

    size_t                      cur_fn;
    stack_checker               vstack;
    std::vector<bc_instruction> bytecode;

    lauf_signature signature() const
    {
        return functions[cur_fn].sig;
    }
};

lauf_builder lauf_build(const char* mod_name)
{
    return new lauf_builder_impl{mod_name};
}

lauf_module lauf_build_finish(lauf_builder b)
{
    for (auto [caller, bc_index, callee] : b->call_patches)
    {
        auto constant_idx = b->constants.insert(b->functions[callee].fn);
        auto bytecode     = const_cast<bc_instruction*>(b->functions[caller].fn->bytecode());
        bytecode[bc_index].call.constant_idx = constant_idx;
    }

    auto mod = lauf_impl_allocate_module(b->constants.size());
    std::memcpy(mod->constant_data(), b->constants.data(),
                b->constants.size() * sizeof(lauf_value));

    auto cur_fn = lauf_function(nullptr);
    for (auto& fn : b->functions)
    {
        if (fn.fn == nullptr)
            continue;

        if (cur_fn != nullptr)
            cur_fn->next_function = fn.fn;
        else
            mod->first_function = fn.fn;
    }

    return mod;
}

lauf_builder_function lauf_build_declare_function(lauf_builder b, const char* fn_name,
                                                  lauf_signature signature)
{
    auto idx = b->functions.size();
    b->functions.push_back({fn_name, signature, nullptr});
    return {idx};
}

void lauf_build_start_function(lauf_builder b, lauf_builder_function fn)
{
    std::move(b->vstack).reset(); // NOLINT
    b->bytecode.clear();
    b->cur_fn = fn._fn;
}

lauf_function lauf_build_finish_function(lauf_builder b)
{
    LAUF_VERIFY(b->vstack.cur_stack_size() == 0, "finish_function",
                "stack not empty on function finish");
    LAUF_VERIFY(b->vstack.max_stack_size() <= UINT16_MAX, "finish_function",
                "max function stack size exceeds 16 bit limit");

    auto& fn_decl       = b->functions[b->cur_fn];
    auto  fn            = lauf_impl_allocate_function(b->bytecode.size());
    fn->next_function   = nullptr;
    fn->name            = fn_decl.name;
    fn->max_vstack_size = b->vstack.max_stack_size();
    fn->input_count     = fn_decl.sig.input_count;
    fn->output_count    = fn_decl.sig.output_count;
    std::memcpy(const_cast<bc_instruction*>(fn->bytecode()), b->bytecode.data(),
                b->bytecode.size() * sizeof(uint32_t));
    fn_decl.fn = fn;
    return fn;
}

lauf_builder_if lauf_build_if(lauf_builder b, lauf_condition condition)
{
    lauf_builder_if if_;
    if_._jump_end = std::size_t(-1);
    if_._jump_if  = b->bytecode.size();

    // We generate a jump for the else, so negate the condition.
    bc_instruction inst;
    inst.jump_if.op     = bc_op::jump_if;
    inst.jump_if.offset = 0;
    switch (condition)
    {
    case LAUF_IF_ZERO:
        inst.jump_if.cc = condition_code::if_nonzero;
        break;
    case LAUF_IF_NONZERO:
        inst.jump_if.cc = condition_code::if_zero;
        break;
    }
    b->bytecode.push_back(inst);

    LAUF_VERIFY_RESULT(b->vstack.pop(), "if", "missing value for condition");
    LAUF_VERIFY(b->vstack.cur_stack_size() == 0, "if", "trailing values after if condition");
    return if_;
}

void lauf_build_else(lauf_builder b, lauf_builder_if* if_)
{
    // Create jump that skips the else.
    if_->_jump_end = b->bytecode.size();
    b->bytecode.push_back(LAUF_BC_INSTRUCTION(jump, 0));

    // Patch jump_if to current position.
    b->bytecode[if_->_jump_if].jump_if.offset = b->bytecode.size() - if_->_jump_if;

    LAUF_VERIFY(b->vstack.cur_stack_size() == 0, "else", "trailing values after if block");
}

void lauf_build_end_if(lauf_builder b, lauf_builder_if* if_)
{
    if (if_->_jump_end == std::size_t(-1)) // no else
    {
        // Patch jump_if to current position.
        b->bytecode[if_->_jump_if].jump_if.offset = b->bytecode.size() - if_->_jump_if;
    }
    else
    {
        // Patch jump_end to current position.
        // (jump_if already patched as part of the else).
        b->bytecode[if_->_jump_end].jump.offset = b->bytecode.size() - if_->_jump_end;
    }

    LAUF_VERIFY(b->vstack.cur_stack_size() == 0, "end_if", "trailing values after else block");
}

void lauf_build_return(lauf_builder b)
{
    b->bytecode.push_back(LAUF_BC_INSTRUCTION(return_));
    LAUF_VERIFY_RESULT(b->vstack.pop(b->signature().output_count), "return",
                       "missing values for return");
}

void lauf_build_int(lauf_builder b, lauf_value_int value)
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
        auto idx = b->constants.insert(value);
        b->bytecode.push_back(LAUF_BC_INSTRUCTION(push, idx));
    }

    b->vstack.push();
}

void lauf_build_argument(lauf_builder b, size_t idx)
{
    LAUF_VERIFY(idx < b->signature().input_count, "argument", "argument index out of range");

    b->bytecode.push_back(LAUF_BC_INSTRUCTION(argument, uint32_t(idx)));
    b->vstack.push();
}

void lauf_build_pop(lauf_builder b, size_t n)
{
    if (n == 1)
        b->bytecode.push_back(LAUF_BC_INSTRUCTION(pop_one));
    else
        b->bytecode.push_back(LAUF_BC_INSTRUCTION(pop, uint32_t(n)));

    LAUF_VERIFY_RESULT(b->vstack.pop(n), "pop", "not enough values to pop");
}

void lauf_build_call(lauf_builder b, lauf_function fn)
{
    auto idx = b->constants.insert(fn);
    b->bytecode.push_back(LAUF_BC_INSTRUCTION(call, idx));

    LAUF_VERIFY_RESULT(b->vstack.pop(fn->input_count), "call", "missing arguments for call");
    b->vstack.push(fn->output_count);
}

void lauf_build_call_decl(lauf_builder b, lauf_builder_function fn)
{
    auto index = b->bytecode.size();
    b->bytecode.push_back(LAUF_BC_INSTRUCTION(call, bc_constant_idx{}));

    auto fn_decl = b->functions[fn._fn];
    LAUF_VERIFY_RESULT(b->vstack.pop(fn_decl.sig.input_count), "call",
                       "missing arguments for call");
    b->vstack.push(fn_decl.sig.output_count);

    b->call_patches.push_back({b->cur_fn, index, fn._fn});
}

void lauf_build_call_builtin(lauf_builder b, struct lauf_builtin fn)
{
    auto idx = b->constants.insert(reinterpret_cast<void*>(fn.impl));
    b->bytecode.push_back(LAUF_BC_INSTRUCTION(call_builtin, idx));

    LAUF_VERIFY_RESULT(b->vstack.pop(fn.signature.input_count), "call_builtin",
                       "missing arguments for call");
    b->vstack.push(fn.signature.output_count);
}

