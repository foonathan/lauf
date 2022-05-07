// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/vm.h>

#include "detail/bytecode.hpp"
#include "impl/module.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <lauf/builtin.h>
#include <new>
#include <type_traits>

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
class stack_allocator
{
public:
    explicit stack_allocator(unsigned char* memory, size_t size) : _cur(memory), _end(_cur + size)
    {}

    const void* marker() const
    {
        return _cur;
    }
    void unwind(const void* marker)
    {
        _cur = (unsigned char*)(marker);
    }

    template <typename T>
    bool push(const T& object)
    {
        static_assert(std::is_trivially_copyable_v<T>);

        if (_cur + sizeof(T) > _end)
            return false;

        std::memcpy(_cur, &object, sizeof(T));
        _cur += sizeof(T);
        return true;
    }

    template <typename T>
    void pop(T& result)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        std::memcpy(&result, _cur -= sizeof(T), sizeof(T));
    }
    template <typename T>
    T pop()
    {
        T result;
        pop(result);
        return result;
    }

    void* allocate(std::size_t size, std::size_t alignment)
    {
        auto misaligned   = reinterpret_cast<std::uintptr_t>(_cur) & (alignment - 1);
        auto align_offset = misaligned != 0 ? (alignment - misaligned) : 0;
        if (_cur + align_offset + size > _end)
            return nullptr;

        _cur += align_offset;
        auto memory = _cur;
        _cur += size;
        return memory;
    }
    template <typename T>
    T* allocate(std::size_t n = 1)
    {
        return static_cast<T*>(allocate(n * sizeof(T), alignof(T)));
    }

private:
    unsigned char* _cur;
    unsigned char* _end;
};

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
    {}
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

        case bc_op::pop: {
            auto count = inst.argument.constant;
            frame.vstack_ptr -= count;
            ++frame.ip;
            break;
        }
        case bc_op::pop_one: {
            frame.vstack_ptr--;
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
            auto idx      = inst.call_builtin.constant_idx;
            auto callback = reinterpret_cast<lauf_builtin_function*>(mod->get_constant(idx).as_ptr);
            frame.vstack_ptr = callback(frame.vstack_ptr);
            ++frame.ip;
            break;
        }
        }
    }
}

