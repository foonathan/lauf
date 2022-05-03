// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/vm.h>

#include "detail/bytecode.hpp"
#include "detail/function.hpp"
#include <vector>

struct lauf_VMImpl
{
    std::vector<lauf_Value> stack;
};

lauf_VM lauf_vm(void)
{
    return new lauf_VMImpl{};
}

void lauf_vm_destroy(lauf_VM vm)
{
    delete vm;
}

void lauf_vm_execute(lauf_VM vm, lauf_Function fn, const lauf_Value* input, lauf_Value* output)
{
    vm->stack.resize(fn->max_stack_size);
    auto stack_ptr = vm->stack.data();

    std::memcpy(stack_ptr, input, sizeof(lauf_Value) * fn->signature.input_count);
    stack_ptr += fn->signature.input_count;

    auto ip = fn->bytecode_begin();
    while (ip != fn->bytecode_end())
    {
        switch (lauf::op(*ip))
        {
        case lauf::op::push: {
            auto constant = (ip[1] << 8) | ip[2];
            *stack_ptr++  = fn->constant_begin()[constant];
            ip += 3;
            break;
        }
        case lauf::op::pop: {
            auto count = (ip[1] << 8) | ip[2];
            stack_ptr -= count;
            ip += 3;
            break;
        }
        }
    }

    stack_ptr -= fn->signature.output_count;
    std::memcpy(output, stack_ptr, sizeof(lauf_Value) * fn->signature.output_count);
}

