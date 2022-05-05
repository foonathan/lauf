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
    auto constants = fn->constants();

    vm->stack.resize(fn->max_stack_size);
    auto stack_ptr = vm->stack.data();

    std::memcpy(stack_ptr, input, sizeof(lauf_Value) * fn->input_count);
    stack_ptr += fn->input_count;

    for (auto ip = fn->bytecode_begin(); ip != fn->bytecode_end();)
    {
        auto inst = *ip;
        switch (LAUF_BC_OP(inst))
        {
        case lauf::op::jump: {
            auto offset = LAUF_BC_PAYLOAD(inst);
            ip += offset;
            break;
        }
        case lauf::op::jump_if: {
            auto payload = LAUF_BC_PAYLOAD(inst);
            auto cc      = static_cast<lauf::condition_code>(payload & 0b111);
            auto offset  = static_cast<size_t>(payload >> 3);

            auto top = *--stack_ptr;
            switch (cc)
            {
            case lauf::condition_code::if_zero:
                if (top.as_int == 0)
                    ip += offset;
                else
                    ++ip;
                break;
            case lauf::condition_code::if_nonzero:
                if (top.as_int != 0)
                    ip += offset;
                else
                    ++ip;
                break;
            case lauf::condition_code::cmp_lt:
                if (top.as_int < 0)
                    ip += offset;
                else
                    ++ip;
                break;
            case lauf::condition_code::cmp_le:
                if (top.as_int <= 0)
                    ip += offset;
                else
                    ++ip;
                break;
            case lauf::condition_code::cmp_gt:
                if (top.as_int > 0)
                    ip += offset;
                else
                    ++ip;
                break;
            case lauf::condition_code::cmp_ge:
                if (top.as_int >= 0)
                    ip += offset;
                else
                    ++ip;
                break;
            }
            break;
        }

        case lauf::op::push: {
            auto idx     = LAUF_BC_PAYLOAD(inst);
            *stack_ptr++ = constants[idx];
            ++ip;
            break;
        }
        case lauf::op::push_zero: {
            *stack_ptr++ = lauf_Value{};
            ++ip;
            break;
        }
        case lauf::op::push_small_zext: {
            lauf_Value constant;
            constant.as_int = LAUF_BC_PAYLOAD(inst);
            *stack_ptr++    = constant;
            ++ip;
            break;
        }
        case lauf::op::push_small_neg: {
            lauf_Value constant;
            constant.as_int = -lauf_ValueInt(LAUF_BC_PAYLOAD(inst));
            *stack_ptr++    = constant;
            ++ip;
            break;
        }

        case lauf::op::pop: {
            auto count = LAUF_BC_PAYLOAD(inst);
            stack_ptr -= count;
            ++ip;
            break;
        }
        case lauf::op::pop_one: {
            stack_ptr--;
            ++ip;
            break;
        }

        case lauf::op::call_builtin: {
            auto idx      = LAUF_BC_PAYLOAD(inst);
            auto callback = reinterpret_cast<lauf_BuiltinFunctionCallback*>(constants[idx].as_ptr);
            stack_ptr     = callback(stack_ptr);
            ++ip;
            break;
        }
        }
    }

    stack_ptr -= fn->output_count;
    std::memcpy(output, stack_ptr, sizeof(lauf_Value) * fn->output_count);
}

