// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/vm.h>

#include "detail/bytecode.hpp"
#include "detail/function.hpp"

const lauf_VMOptions lauf_default_vm_options = {
    lauf_default_error_handler,
    size_t(1) * 1024 * 1024,
};

struct alignas(std::max_align_t) lauf_VMImpl
{
    lauf_ErrorHandler handler;
    size_t            stack_size;

    unsigned char* stack_begin()
    {
        return reinterpret_cast<unsigned char*>(this + 1);
    }
};

lauf_VM lauf_vm(lauf_VMOptions options)
{
    auto memory = ::operator new(sizeof(lauf_VMImpl) + options.max_stack_size);
    return ::new (memory) lauf_VMImpl{options.error_handler, options.max_stack_size};
}

void lauf_vm_destroy(lauf_VM vm)
{
    ::operator delete(vm);
}

void lauf_vm_execute(lauf_VM vm, lauf_Function fn, const lauf_Value* input, lauf_Value* output)
{
    auto stack_ptr = vm->stack_begin();
    auto stack_end = vm->stack_begin() + vm->stack_size;

    auto constants = fn->constants();
    auto ip        = fn->bytecode_begin();

    auto vstack_ptr = reinterpret_cast<lauf_Value*>(stack_ptr);
    stack_ptr += fn->max_vstack_size;
    if (stack_ptr > stack_end)
    {
        vm->handler.stack_overflow({fn->name, "call"}, vm->stack_size);
        return;
    }

    while (true)
    {
        auto inst = *ip;
        switch (LAUF_BC_OP(inst))
        {
        case lauf::op::return_: {
            vstack_ptr -= fn->output_count;
            std::memcpy(output, vstack_ptr, sizeof(lauf_Value) * fn->output_count);
            return;
        }
        case lauf::op::jump: {
            auto offset = LAUF_BC_PAYLOAD(inst);
            ip += offset;
            break;
        }
        case lauf::op::jump_if: {
            auto payload = LAUF_BC_PAYLOAD(inst);
            auto cc      = static_cast<lauf::condition_code>(payload & 0b111);
            auto offset  = static_cast<size_t>(payload >> 3);

            auto top = *--vstack_ptr;
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
            auto idx      = LAUF_BC_PAYLOAD(inst);
            *vstack_ptr++ = constants[idx];
            ++ip;
            break;
        }
        case lauf::op::push_zero: {
            *vstack_ptr++ = lauf_Value{};
            ++ip;
            break;
        }
        case lauf::op::push_small_zext: {
            lauf_Value constant;
            constant.as_int = LAUF_BC_PAYLOAD(inst);
            *vstack_ptr++   = constant;
            ++ip;
            break;
        }
        case lauf::op::push_small_neg: {
            lauf_Value constant;
            constant.as_int = -lauf_ValueInt(LAUF_BC_PAYLOAD(inst));
            *vstack_ptr++   = constant;
            ++ip;
            break;
        }

        case lauf::op::argument: {
            auto idx      = LAUF_BC_PAYLOAD(inst);
            *vstack_ptr++ = input[idx];
            ++ip;
            break;
        }

        case lauf::op::pop: {
            auto count = LAUF_BC_PAYLOAD(inst);
            vstack_ptr -= count;
            ++ip;
            break;
        }
        case lauf::op::pop_one: {
            vstack_ptr--;
            ++ip;
            break;
        }

        case lauf::op::call_builtin: {
            auto idx      = LAUF_BC_PAYLOAD(inst);
            auto callback = reinterpret_cast<lauf_BuiltinFunctionCallback*>(constants[idx].as_ptr);
            vstack_ptr    = callback(vstack_ptr);
            ++ip;
            break;
        }
        }
    }
}

