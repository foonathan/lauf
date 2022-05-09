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
    size_t(512) * 1024,
};

struct alignas(lauf_value) lauf_vm_impl
{
    size_t          value_stack_size;
    stack_allocator memory_stack;

    lauf_value* value_stack()
    {
        return reinterpret_cast<lauf_value*>(this + 1);
    }
};

lauf_vm lauf_vm_create(lauf_vm_options options)
{
    auto memory = ::operator new(sizeof(lauf_vm_impl) + options.max_value_stack_size);
    return ::new (memory) lauf_vm_impl{options.max_value_stack_size};
}

void lauf_vm_destroy(lauf_vm vm)
{
    ::operator delete(vm);
}

namespace
{
struct stack_frame
{
    bc_instruction*         return_ip;
    stack_allocator::marker unwind;
    stack_frame*            prev;
    void*                   locals;
};

} // namespace

void lauf_vm_execute(lauf_vm vm, lauf_module mod, lauf_function fn, const lauf_value* input,
                     lauf_value* output)
{
    auto vstack_ptr = vm->value_stack();
    std::memcpy(vstack_ptr, input, fn->input_count * sizeof(lauf_value));
    vstack_ptr += fn->input_count;

    auto frame = stack_frame{nullptr, vm->memory_stack.top(), nullptr, nullptr};
    frame.prev = &frame;

    auto locals = vm->memory_stack.allocate(fn->local_stack_size, alignof(std::max_align_t));
    auto ip     = fn->bytecode();
    while (ip != nullptr)
    {
        auto inst = *ip;
        switch (inst.tag.op)
        {
        case bc_op::nop:
            ++ip;
            break;

        case bc_op::return_: {
            auto marker = frame.unwind;
            ip          = frame.return_ip;
            locals      = frame.locals;
            frame       = *frame.prev;
            vm->memory_stack.unwind(marker);
            break;
        }
        case bc_op::call: {
            auto callee = mod->get_function(inst.call.function_idx);

            auto marker     = vm->memory_stack.top();
            auto prev_frame = vm->memory_stack.push(frame);
            frame           = stack_frame{ip + 1, marker, prev_frame, locals};

            ip     = callee->bytecode();
            locals = vm->memory_stack.allocate(callee->local_stack_size, alignof(std::max_align_t));
            break;
        }

        case bc_op::jump:
            ip += inst.jump.offset;
            break;
        case bc_op::jump_if: {
            auto top_value = vstack_ptr[-1];
            --vstack_ptr;
            switch (inst.jump_if.cc)
            {
            case condition_code::if_zero:
                if (top_value.as_int == 0)
                    ip += inst.jump_if.offset;
                else
                    ++ip;
                break;
            case condition_code::if_nonzero:
                if (top_value.as_int != 0)
                    ip += inst.jump_if.offset;
                else
                    ++ip;
                break;
            case condition_code::cmp_lt:
                if (top_value.as_int < 0)
                    ip += inst.jump_if.offset;
                else
                    ++ip;
                break;
            case condition_code::cmp_le:
                if (top_value.as_int <= 0)
                    ip += inst.jump_if.offset;
                else
                    ++ip;
                break;
            case condition_code::cmp_gt:
                if (top_value.as_int > 0)
                    ip += inst.jump_if.offset;
                else
                    ++ip;
                break;
            case condition_code::cmp_ge:
                if (top_value.as_int >= 0)
                    ip += inst.jump_if.offset;
                else
                    ++ip;
                break;
            }
            break;
        }

        case bc_op::push:
            *vstack_ptr = mod->get_constant(inst.push.constant_idx);
            ++vstack_ptr;
            ++ip;
            break;
        case bc_op::push_zero:
            vstack_ptr->as_int = 0;
            ++vstack_ptr;
            ++ip;
            break;
        case bc_op::push_small_zext:
            vstack_ptr->as_int = inst.push_small_zext.constant;
            ++vstack_ptr;
            ++ip;
            break;
        case bc_op::push_small_neg:
            vstack_ptr->as_int = -lauf_value_int(inst.push_small_zext.constant);
            ++vstack_ptr;
            ++ip;
            break;
        case bc_op::local_addr:
            vstack_ptr->as_ptr
                = static_cast<unsigned char*>(locals) + ptrdiff_t(inst.local_addr.constant);
            ++vstack_ptr;
            ++ip;
            break;

        case bc_op::drop:
            vstack_ptr -= ptrdiff_t(inst.drop.constant);
            ++ip;
            break;
        case bc_op::pick: {
            auto index  = ptrdiff_t(inst.pick.constant);
            auto value  = vstack_ptr[-index - 1];
            *vstack_ptr = value;
            ++vstack_ptr;
            ++ip;
            break;
        }
        case bc_op::dup: {
            *vstack_ptr = vstack_ptr[-1];
            ++vstack_ptr;
            ++ip;
            break;
        }
        case bc_op::roll: {
            auto index = ptrdiff_t(inst.pick.constant);
            auto value = vstack_ptr[-index - 1];
            std::memmove(vstack_ptr - index - 1, vstack_ptr - index, index * sizeof(lauf_value));
            vstack_ptr[-1] = value;
            ++ip;
            break;
        }
        case bc_op::swap: {
            std::swap(vstack_ptr[-1], vstack_ptr[-2]);
            ++ip;
            break;
        }

        case bc_op::call_builtin: {
            auto callee = reinterpret_cast<lauf_builtin_function*>(
                mod->get_constant(inst.call_builtin.constant_idx).as_ptr);
            vstack_ptr = callee(vstack_ptr);
            ++ip;
            break;
        }

        case bc_op::array_element: {
            auto idx       = vstack_ptr[-2].as_uint;
            auto elem_size = ptrdiff_t(inst.array_element.constant);
            auto addr      = vstack_ptr[-1].as_ptr;

            --vstack_ptr;
            vstack_ptr[-1].as_ptr = static_cast<unsigned char*>(addr) + idx * elem_size;

            ++ip;
            break;
        }
        case bc_op::load_field: {
            auto type
                = static_cast<lauf_type>(mod->get_constant(inst.load_field.constant_idx).as_ptr);

            auto object    = vstack_ptr[-1].as_ptr;
            vstack_ptr[-1] = type->load_field(object, inst.load_field.field);

            ++ip;
            break;
        }
        case bc_op::store_field:
            auto type
                = static_cast<lauf_type>(mod->get_constant(inst.store_field.constant_idx).as_ptr);

            auto object = vstack_ptr[-1].as_ptr;
            type->store_field(object, inst.store_field.field, vstack_ptr[-2]);
            vstack_ptr -= 2;

            ++ip;
            break;
        }
    }

    vstack_ptr -= fn->output_count;
    std::memcpy(output, vstack_ptr, fn->output_count * sizeof(lauf_value));

    vm->memory_stack.reset();
}

