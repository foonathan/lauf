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
    lauf_FunctionSignature sig;
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

void lauf_builder_start_function(lauf_Builder b, const char* name, lauf_FunctionSignature sig)
{
    b->handler.errors = false;

    b->name = name;
    b->sig  = sig;
    std::move(b->stack).reset(); // NOLINT
    std::move(b->constants).reset();
    std::move(b->bytecode).reset();

    b->stack.push(b->sig.input_count);
}

lauf_Function lauf_builder_finish_function(lauf_Builder b)
{
    LAUF_ERROR_CONTEXT(return );
    b->stack.pop(b->handler, ctx, b->sig.output_count);

    if (b->handler.errors)
        return nullptr;
    else
        return lauf::create_function(b->name, b->sig, b->stack.max_stack_size(),
                                     b->constants.data(), b->constants.size(), //
                                     b->bytecode.data(), b->bytecode.size());
}

void lauf_builder_push_int(lauf_Builder b, lauf_ValueInt value)
{
    LAUF_ERROR_CONTEXT(push_int);

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

void lauf_builder_pop(lauf_Builder b, size_t n)
{
    LAUF_ERROR_CONTEXT(pop);

    if (n == 1)
        b->bytecode.op(lauf::op::pop_one);
    else
        b->bytecode.op(b->handler, ctx, lauf::op::pop, n);

    b->stack.pop(b->handler, ctx, n);
}

void lauf_builder_call(lauf_Builder b, lauf_Function fn)
{
    LAUF_ERROR_CONTEXT(call);
    assert(fn->is_builtin); // unimplemented

    auto idx = b->constants.insert(reinterpret_cast<void*>(fn->builtin.fn));
    b->bytecode.op(b->handler, ctx, lauf::op::call_builtin, idx);

    auto [input_count, output_count] = lauf_function_signature(fn);
    b->stack.pop(b->handler, ctx, input_count);
    b->stack.push(output_count);
}

