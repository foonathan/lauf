// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/builder.h>

#include "detail/bytecode.hpp"
#include "detail/function.hpp"
#include "detail/stack_check.hpp"
#include <vector>

struct lauf_BuilderImpl
{
    lauf_ErrorHandler handler;

    const char*             name;
    lauf_FunctionSignature  sig;
    lauf::stack_checker     stack;
    std::vector<lauf_Value> constants;
    lauf::bytecode_builder  bytecode;

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
    b->constants.clear();
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

    lauf_Value constant;
    constant.as_int = value;

    auto idx = b->constants.size();
    b->constants.push_back(constant);

    b->bytecode.op(lauf::op::push);
    b->bytecode.uint16(b->handler, ctx, idx);

    b->stack.push();
}

void lauf_builder_pop(lauf_Builder b, size_t n)
{
    LAUF_ERROR_CONTEXT(pop);

    b->bytecode.op(lauf::op::pop);
    b->bytecode.uint16(b->handler, ctx, n);

    b->stack.pop(b->handler, ctx, n);
}

