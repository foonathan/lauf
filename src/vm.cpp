// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/vm.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <lauf/builtin.h>
#include <lauf/detail/bytecode.hpp>
#include <lauf/detail/stack_allocator.hpp>
#include <lauf/impl/module.hpp>
#include <lauf/type.h>
#include <new>
#include <type_traits>
#include <utility>

using namespace lauf::_detail;

const lauf_vm_options lauf_default_vm_options = {
    size_t(1) * 1024 * 1024,
};

struct alignas(std::max_align_t) lauf_vm_impl
{
    size_t stack_size;

    unsigned char* stack_begin()
    {
        return reinterpret_cast<unsigned char*>(this + 1);
    }
};

lauf_vm lauf_vm_create(lauf_vm_options options)
{
    auto memory = ::operator new(sizeof(lauf_vm_impl) + options.max_stack_size);
    return ::new (memory) lauf_vm_impl{options.max_stack_size};
}

void lauf_vm_destroy(lauf_vm vm)
{
    ::operator delete(vm);
}

namespace
{
struct stack_frame
{
    const void*           base;
    lauf_function         fn;
    const bc_instruction* ip;
    const lauf_value*     arguments;
    lauf_value*           vstack_ptr;

    stack_frame(lauf_value* output)
    : base(nullptr), fn(nullptr), ip(nullptr), arguments(nullptr), vstack_ptr(output)
    {}

    stack_frame(stack_allocator& stack, lauf_function fn, const lauf_value* arguments)
    : base(stack.marker()), fn(fn), ip(fn->bytecode()), arguments(arguments),
      vstack_ptr(stack.allocate<lauf_value>(fn->max_vstack_size))
    {
        stack.allocate(fn->local_stack_size, alignof(lauf_value));
    }

    std::size_t local_variable_offset(const stack_allocator& stack) const
    {
        return stack.to_address(base) + fn->max_vstack_size * sizeof(lauf_value);
    }
};
} // namespace

void lauf_vm_execute(lauf_vm vm, lauf_module mod, lauf_function fn, const lauf_value* input,
                     lauf_value* output)
{
    stack_allocator stack(vm->stack_begin(), vm->stack_size);
    stack.push(stack_frame(output));

    stack_frame frame(stack, fn, input);
    while (frame.ip != nullptr)
    {
        auto inst = *frame.ip;
        switch (inst.tag.op)
        {
        case bc_op::nop: {
            ++frame.ip;
            break;
        }
        case bc_op::return_: {
            auto output_count = frame.fn->output_count;
            auto outputs      = frame.vstack_ptr - output_count;

            stack.unwind(frame.base);
            stack.pop<stack_frame>(frame);

            std::memcpy(frame.vstack_ptr, outputs, sizeof(lauf_value) * output_count);
            frame.vstack_ptr += output_count;
            break;
        }
        case bc_op::jump: {
            frame.ip += inst.jump.offset;
            break;
        }
        case bc_op::jump_if: {
            auto offset = inst.jump_if.offset;
            auto top    = *--frame.vstack_ptr;
            switch (inst.jump_if.cc)
            {
            case condition_code::if_zero:
                if (top.as_int == 0)
                    frame.ip += offset;
                else
                    ++frame.ip;
                break;
            case condition_code::if_nonzero:
                if (top.as_int != 0)
                    frame.ip += offset;
                else
                    ++frame.ip;
                break;
            case condition_code::cmp_lt:
                if (top.as_int < 0)
                    frame.ip += offset;
                else
                    ++frame.ip;
                break;
            case condition_code::cmp_le:
                if (top.as_int <= 0)
                    frame.ip += offset;
                else
                    ++frame.ip;
                break;
            case condition_code::cmp_gt:
                if (top.as_int > 0)
                    frame.ip += offset;
                else
                    ++frame.ip;
                break;
            case condition_code::cmp_ge:
                if (top.as_int >= 0)
                    frame.ip += offset;
                else
                    ++frame.ip;
                break;
            }
            break;
        }

        case bc_op::push: {
            *frame.vstack_ptr++ = mod->get_constant(inst.push.constant_idx);
            ++frame.ip;
            break;
        }
        case bc_op::push_zero: {
            *frame.vstack_ptr++ = lauf_value{};
            ++frame.ip;
            break;
        }
        case bc_op::push_small_zext: {
            lauf_value constant;
            constant.as_int     = inst.push_small_zext.constant;
            *frame.vstack_ptr++ = constant;
            ++frame.ip;
            break;
        }
        case bc_op::push_small_neg: {
            lauf_value constant;
            constant.as_int     = -lauf_value_int(inst.push_small_neg.constant);
            *frame.vstack_ptr++ = constant;
            ++frame.ip;
            break;
        }

        case bc_op::argument: {
            auto idx            = inst.argument.constant;
            *frame.vstack_ptr++ = frame.arguments[idx];
            ++frame.ip;
            break;
        }
        case bc_op::local_addr: {
            frame.vstack_ptr->as_address
                = frame.local_variable_offset(stack) + inst.local_addr.constant;
            ++frame.vstack_ptr;
            ++frame.ip;
            break;
        }

        case bc_op::drop: {
            auto count = inst.drop.constant;
            frame.vstack_ptr -= count;
            ++frame.ip;
            break;
        }
        case bc_op::pick: {
            auto index          = ptrdiff_t(inst.pick.constant);
            auto value          = frame.vstack_ptr[-index - 1];
            *frame.vstack_ptr++ = value;
            ++frame.ip;
            break;
        }
        case bc_op::dup: {
            *frame.vstack_ptr = frame.vstack_ptr[-1];
            ++frame.vstack_ptr;
            ++frame.ip;
            break;
        }
        case bc_op::roll: {
            auto index = ptrdiff_t(inst.roll.constant);

            // Remember the new element that is on top of the stack.
            auto value = frame.vstack_ptr[-index - 1];
            // Move [-index, 0] to [-index - 1, -1]
            std::memmove(frame.vstack_ptr - index - 1, frame.vstack_ptr - index,
                         index * sizeof(lauf_value));
            // Add the new item on the top.
            frame.vstack_ptr[-1] = value;

            ++frame.ip;
            break;
        }
        case bc_op::swap: {
            std::swap(frame.vstack_ptr[-1], frame.vstack_ptr[-2]);
            ++frame.ip;
            break;
        }

        case bc_op::call: {
            auto callee = mod->get_function(inst.call.function_idx);

            auto args = (frame.vstack_ptr -= callee->input_count);
            ++frame.ip;

            stack.push(frame);
            frame = stack_frame(stack, callee, args);
            break;
        }
        case bc_op::call_builtin: {
            auto idx         = inst.call_builtin.constant_idx;
            auto callback    = (lauf_builtin_function*)(mod->get_constant(idx).as_native_ptr);
            frame.vstack_ptr = callback(frame.vstack_ptr);
            ++frame.ip;
            break;
        }

        case bc_op::load_field: {
            auto type_idx = inst.load_field.constant_idx;
            auto type     = static_cast<lauf_type>(mod->get_constant(type_idx).as_native_ptr);

            auto object          = stack.to_pointer(frame.vstack_ptr[-1].as_address);
            frame.vstack_ptr[-1] = type->load_field(object, inst.load_field.field);

            ++frame.ip;
            break;
        }
        case bc_op::store_field: {
            auto type_idx = inst.store_field.constant_idx;
            auto type     = static_cast<lauf_type>(mod->get_constant(type_idx).as_native_ptr);

            auto object = stack.to_pointer(frame.vstack_ptr[-1].as_address);
            type->store_field(object, inst.store_field.field, frame.vstack_ptr[-2]);
            frame.vstack_ptr -= 2;

            ++frame.ip;
            break;
        }
        }
    }
}

