// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/builder.h>

#include "detail/bytecode.hpp"
#include "detail/constant_pool.hpp"
#include "detail/stack_check.hpp"
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
    lauf_error_handler handler;

    const char*                mod_name;
    constant_pool              constants;
    std::vector<function_decl> functions;
    std::vector<call_patch>    call_patches;

    size_t                      cur_fn;
    lauf::stack_checker         vstack;
    std::vector<bc_instruction> bytecode;

    lauf_signature signature() const
    {
        return functions[cur_fn].sig;
    }
};

#define LAUF_ERROR_CONTEXT(Instruction)                                                            \
    [[maybe_unused]] const lauf_error_context ctx                                                  \
    {                                                                                              \
        b->functions[b->cur_fn].name, #Instruction                                                 \
    }

lauf_builder lauf_build(const char* mod_name)
{
    return new lauf_builder_impl{lauf_default_error_handler, mod_name};
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
    b->handler.errors = false;

    std::move(b->vstack).reset(); // NOLINT
    b->bytecode.clear();
    b->cur_fn = fn._fn;
}

lauf_function lauf_build_finish_function(lauf_builder b)
{
    LAUF_ERROR_CONTEXT(end_function);

    b->vstack.assert_empty(b->handler, ctx);

    if (b->vstack.max_stack_size() > UINT16_MAX)
    {
        b->handler.encoding_error(ctx, 16, b->vstack.max_stack_size());
        b->handler.errors = true;
    }

    if (b->handler.errors)
        return nullptr;

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
    LAUF_ERROR_CONTEXT(if);

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

    b->vstack.pop(b->handler, ctx);
    b->vstack.assert_empty(b->handler, ctx);
    return if_;
}

void lauf_build_else(lauf_builder b, lauf_builder_if* if_)
{
    LAUF_ERROR_CONTEXT(else);

    // Create jump that skips the else.
    if_->_jump_end = b->bytecode.size();
    b->bytecode.push_back(LAUF_BC_INSTRUCTION(jump, 0));

    // Patch jump_if to current position.
    b->bytecode[if_->_jump_if].jump_if.offset = b->bytecode.size() - if_->_jump_if;

    b->vstack.assert_empty(b->handler, ctx);
}

void lauf_build_end_if(lauf_builder b, lauf_builder_if* if_)
{
    LAUF_ERROR_CONTEXT(end_if);

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

    b->vstack.assert_empty(b->handler, ctx);
}

void lauf_build_return(lauf_builder b)
{
    LAUF_ERROR_CONTEXT(return );

    b->bytecode.push_back(LAUF_BC_INSTRUCTION(return_));
    b->vstack.pop(b->handler, ctx, b->signature().output_count);
}

void lauf_build_int(lauf_builder b, lauf_value_int value)
{
    LAUF_ERROR_CONTEXT(int);

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
    LAUF_ERROR_CONTEXT(argument);
    if (idx >= b->signature().input_count)
    {
        b->handler.errors = true;
        b->handler.index_error(ctx, b->signature().input_count, idx);
    }

    b->bytecode.push_back(LAUF_BC_INSTRUCTION(argument, uint32_t(idx)));
    b->vstack.push();
}

void lauf_build_pop(lauf_builder b, size_t n)
{
    LAUF_ERROR_CONTEXT(pop);

    if (n == 1)
        b->bytecode.push_back(LAUF_BC_INSTRUCTION(pop_one));
    else
        b->bytecode.push_back(LAUF_BC_INSTRUCTION(pop, uint32_t(n)));

    b->vstack.pop(b->handler, ctx, n);
}

void lauf_build_call(lauf_builder b, lauf_function fn)
{
    LAUF_ERROR_CONTEXT(call);

    auto idx = b->constants.insert(fn);
    b->bytecode.push_back(LAUF_BC_INSTRUCTION(call, idx));

    b->vstack.pop(b->handler, ctx, fn->input_count);
    b->vstack.push(fn->output_count);
}

void lauf_build_call_decl(lauf_builder b, lauf_builder_function fn)
{
    LAUF_ERROR_CONTEXT(call);

    auto index = b->bytecode.size();
    b->bytecode.push_back(LAUF_BC_INSTRUCTION(call, bc_constant_idx{}));

    auto fn_decl = b->functions[fn._fn];
    b->vstack.pop(b->handler, ctx, fn_decl.sig.input_count);
    b->vstack.push(fn_decl.sig.output_count);

    b->call_patches.push_back({b->cur_fn, index, fn._fn});
}

void lauf_build_call_builtin(lauf_builder b, struct lauf_builtin fn)
{
    LAUF_ERROR_CONTEXT(call_builtin);

    auto idx = b->constants.insert(reinterpret_cast<void*>(fn.impl));
    b->bytecode.push_back(LAUF_BC_INSTRUCTION(call_builtin, idx));

    b->vstack.pop(b->handler, ctx, fn.signature.input_count);
    b->vstack.push(fn.signature.output_count);
}

