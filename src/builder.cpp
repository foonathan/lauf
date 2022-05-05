// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/builder.h>

#include "detail/bytecode.hpp"
#include "detail/constant_pool.hpp"
#include "detail/function.hpp"
#include "detail/stack_check.hpp"
#include <cassert>
#include <vector>

struct lauf_BuilderImpl
{
    lauf_ErrorHandler handler;

    const char*            name;
    lauf_Signature         sig;
    lauf::stack_checker    stack;
    lauf::constant_pool    constants;
    lauf::bytecode_builder bytecode;

    lauf_BuilderImpl() : handler(lauf_default_error_handler), name(nullptr), sig{} {}
};

#define LAUF_ERROR_CONTEXT(Instruction)                                                            \
    [[maybe_unused]] const lauf_ErrorContext ctx                                                   \
    {                                                                                              \
        b->name, #Instruction                                                                      \
    }

lauf_Builder lauf_builder(void)
{
    return new lauf_BuilderImpl();
}

void lauf_builder_destroy(lauf_Builder b)
{
    delete b;
}

void lauf_builder_function(lauf_Builder b, const char* name, lauf_Signature sig)
{
    b->handler.errors = false;

    b->name = name;
    b->sig  = sig;
    std::move(b->stack).reset(); // NOLINT
    std::move(b->constants).reset();
    std::move(b->bytecode).reset();
}

lauf_Function lauf_builder_end_function(lauf_Builder b)
{
    LAUF_ERROR_CONTEXT(end_function);

    b->stack.assert_empty(b->handler, ctx);

    if (b->stack.max_stack_size() > UINT16_MAX)
    {
        b->handler.encoding_error(ctx, 16, b->stack.max_stack_size());
        b->handler.errors = true;
    }

    if (b->bytecode.size() > UINT32_MAX)
    {
        b->handler.encoding_error(ctx, 32, b->bytecode.size());
        b->handler.errors = true;
    }

    if (b->handler.errors)
        return nullptr;
    else
        return lauf::create_function(b->name, b->sig, b->stack.max_stack_size(),
                                     b->constants.data(), b->constants.size(), //
                                     b->bytecode.data(), b->bytecode.size());
}

void lauf_builder_if(lauf_Builder b, lauf_BuilderIf* if_, lauf_Condition condition)
{
    LAUF_ERROR_CONTEXT(if);

    if_->_jump_end = std::size_t(-1);

    // We generate a jump for the else, so negate the condition.
    switch (condition)
    {
    case LAUF_IF_ZERO:
        if_->_jump_if = b->bytecode.jump_if(lauf::condition_code::if_nonzero);
        break;
    case LAUF_IF_NONZERO:
        if_->_jump_if = b->bytecode.jump_if(lauf::condition_code::if_zero);
        break;
    }

    b->stack.pop(b->handler, ctx);
    b->stack.assert_empty(b->handler, ctx);
}

void lauf_builder_else(lauf_Builder b, lauf_BuilderIf* if_)
{
    LAUF_ERROR_CONTEXT(else);

    // Create jump that skips the else.
    if_->_jump_end = b->bytecode.jump();
    // Patch jump_if to current position.
    b->bytecode.patch_jump_if(b->handler, ctx, if_->_jump_if, b->bytecode.size());

    b->stack.assert_empty(b->handler, ctx);
}

void lauf_builder_end_if(lauf_Builder b, lauf_BuilderIf* if_)
{
    LAUF_ERROR_CONTEXT(end_if);

    if (if_->_jump_end == std::size_t(-1)) // no else
    {
        // Patch jump_if to current position.
        b->bytecode.patch_jump_if(b->handler, ctx, if_->_jump_if, b->bytecode.size());
    }
    else
    {
        // Patch jump_end to current position.
        // (jump_if already patched as part of the else).
        b->bytecode.patch_jump(b->handler, ctx, if_->_jump_end, b->bytecode.size());
    }

    b->stack.assert_empty(b->handler, ctx);
}

void lauf_builder_return(lauf_Builder b)
{
    LAUF_ERROR_CONTEXT(return );

    b->bytecode.op(lauf::op::return_);
    b->stack.pop(b->handler, ctx, b->sig.output_count);
}

void lauf_builder_int(lauf_Builder b, lauf_ValueInt value)
{
    LAUF_ERROR_CONTEXT(int);

    if (value == 0)
    {
        b->bytecode.op(lauf::op::push_zero);
    }
    else if (0 < value && value <= lauf::UINT24_MAX)
    {
        auto payload = value & lauf::UINT24_MAX;
        b->bytecode.op(b->handler, ctx, lauf::op::push_small_zext, payload);
    }
    else if (-lauf::UINT24_MAX <= value && value < 0)
    {
        auto payload = (-value) & lauf::UINT24_MAX;
        b->bytecode.op(b->handler, ctx, lauf::op::push_small_neg, payload);
    }
    else
    {
        auto idx = b->constants.insert(value);
        b->bytecode.op(b->handler, ctx, lauf::op::push, idx);
    }

    b->stack.push();
}

void lauf_builder_argument(lauf_Builder b, size_t idx)
{
    LAUF_ERROR_CONTEXT(argument);
    if (idx >= b->sig.input_count)
    {
        b->handler.errors = true;
        b->handler.index_error(ctx, b->sig.input_count, idx);
    }

    b->bytecode.op(b->handler, ctx, lauf::op::argument, idx);
    b->stack.push();
}

void lauf_builder_pop(lauf_Builder b, size_t n)
{
    LAUF_ERROR_CONTEXT(pop);

    if (n == 1)
        b->bytecode.op(lauf::op::pop_one);
    else
        b->bytecode.op(b->handler, ctx, lauf::op::pop, n);

    b->stack.pop(b->handler, ctx, n);
}

void lauf_builder_call_builtin(lauf_Builder b, lauf_BuiltinFunction fn)
{
    LAUF_ERROR_CONTEXT(call);

    auto idx = b->constants.insert(reinterpret_cast<void*>(fn.impl));
    b->bytecode.op(b->handler, ctx, lauf::op::call_builtin, idx);

    b->stack.pop(b->handler, ctx, fn.signature.input_count);
    b->stack.push(fn.signature.output_count);
}

