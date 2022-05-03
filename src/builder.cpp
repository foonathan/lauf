// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/builder.h>

#include "detail/bytecode.hpp"
#include "detail/function.hpp"

#include <vector>

struct lauf_BuilderImpl
{
    const char*                name;
    lauf_FunctionSignature     sig;
    std::size_t                cur_stack_size, max_stack_size;
    std::vector<lauf_Value>    constants;
    std::vector<unsigned char> bytecode;

    lauf_BuilderImpl() : name(nullptr), sig{}, cur_stack_size(0), max_stack_size(0) {}
};

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
    b->name           = name;
    b->sig            = sig;
    b->cur_stack_size = b->max_stack_size = 0;
    b->constants.clear();
    b->bytecode.clear();
}

lauf_Function lauf_builder_finish_function(lauf_Builder b)
{
    return lauf::create_function(b->name, b->sig, b->max_stack_size, b->constants.data(),
                                 b->constants.size(), b->bytecode.data(), b->bytecode.size());
}

lauf_StackIndex lauf_builder_push_int(lauf_Builder b, lauf_ValueInt value)
{
    lauf_Value constant;
    constant.as_int = value;

    auto idx = b->constants.size();
    b->constants.push_back(constant);

    b->bytecode.emplace_back(static_cast<unsigned char>(lauf::op::push));
    b->bytecode.emplace_back(idx >> 8);
    b->bytecode.emplace_back(idx & 0xFF);

    auto stack_idx = b->cur_stack_size;
    ++b->cur_stack_size;
    if (b->cur_stack_size > b->max_stack_size)
        b->max_stack_size = b->cur_stack_size;
    return {stack_idx};
}

void lauf_builder_pop(lauf_Builder b, lauf_StackIndex idx)
{
    auto diff = b->cur_stack_size - idx.impl;

    b->bytecode.emplace_back(static_cast<unsigned char>(lauf::op::pop));
    b->bytecode.emplace_back(diff >> 8);
    b->bytecode.emplace_back(diff & 0xFF);
}

