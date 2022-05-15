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
#include <lauf/impl/program.hpp>
#include <lauf/type.h>
#include <new>
#include <type_traits>
#include <utility>

using namespace lauf::_detail;

const lauf_vm_options lauf_default_vm_options
    = {size_t(512) * 1024, [](lauf_panic_info, const char* message) {
           std::fprintf(stderr, "[lauf] panic: %s\n", message);
       }};

struct alignas(lauf_value) lauf_vm_impl
{
    lauf_module        mod;
    lauf_panic_handler panic_handler;
    size_t             value_stack_size;
    stack_allocator    memory_stack;

    lauf_value* value_stack()
    {
        return reinterpret_cast<lauf_value*>(this + 1);
    }
};

lauf_vm lauf_vm_create(lauf_vm_options options)
{
    auto memory = ::operator new(sizeof(lauf_vm_impl) + options.max_value_stack_size);
    return ::new (memory)
        lauf_vm_impl{nullptr, options.panic_handler, options.max_value_stack_size};
}

void lauf_vm_destroy(lauf_vm vm)
{
    ::operator delete(vm);
}

namespace
{
struct alignas(std::max_align_t) stack_frame
{
    bc_instruction*         return_ip;
    stack_allocator::marker unwind;
    stack_frame*            prev;
};

bool check_condition(condition_code cc, lauf_value value)
{
    switch (cc)
    {
    case condition_code::is_zero:
        return value.as_sint == 0;
    case condition_code::is_nonzero:
        return value.as_sint != 0;
    case condition_code::cmp_lt:
        return value.as_sint < 0;
    case condition_code::cmp_le:
        return value.as_sint <= 0;
    case condition_code::cmp_gt:
        return value.as_sint > 0;
    case condition_code::cmp_ge:
        return value.as_sint >= 0;
    }
}
} // namespace

namespace
{
using inst_fn = bool(bc_instruction*, lauf_value*, void*, lauf_vm);

bool dispatch(bc_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr, lauf_vm vm);

#define LAUF_INST_FN(Name)                                                                         \
    bool execute_##Name(bc_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr, lauf_vm vm)

#define LAUF_INST_DISPATCH [[clang::musttail]] return dispatch(ip, vstack_ptr, frame_ptr, vm)

LAUF_INST_FN(return_)
{
    auto frame  = static_cast<stack_frame*>(frame_ptr) - 1;
    auto marker = frame->unwind;

    ip        = frame->return_ip;
    frame_ptr = frame->prev + 1;
    vm->memory_stack.unwind(marker);

    LAUF_INST_DISPATCH;
}
LAUF_INST_FN(call)
{
    auto callee = vm->mod->get_function(ip->call.function_idx);

    auto marker     = vm->memory_stack.top();
    auto prev_frame = static_cast<stack_frame*>(frame_ptr) - 1;
    vm->memory_stack.push(stack_frame{ip + 1, marker, prev_frame});

    ip        = callee->bytecode();
    frame_ptr = vm->memory_stack.allocate(callee->local_stack_size, alignof(std::max_align_t));

    LAUF_INST_DISPATCH;
}
LAUF_INST_FN(call_builtin)
{
    // This isn't the exact type stored, see the definition of `lauf_builtin_dispatch()`.
    auto callee = (inst_fn*)(vm->mod->get_literal(ip->call_builtin.literal_idx).as_ptr);
    ++ip;
    [[clang::musttail]] return callee(ip, vstack_ptr, frame_ptr, vm);
}

LAUF_INST_FN(nop)
{
    ++ip;
    LAUF_INST_DISPATCH;
}
LAUF_INST_FN(jump)
{
    ip += ip->jump.offset;
    LAUF_INST_DISPATCH;
}
LAUF_INST_FN(jump_if)
{
    auto top_value = vstack_ptr[-1];
    if (check_condition(ip->jump_if.cc, top_value))
        ip += ip->jump_if.offset;
    else
        ++ip;
    --vstack_ptr;
    LAUF_INST_DISPATCH;
}

LAUF_INST_FN(push)
{
    *vstack_ptr = vm->mod->get_literal(ip->push.literal_idx);
    ++vstack_ptr;
    ++ip;
    LAUF_INST_DISPATCH;
}
LAUF_INST_FN(push_zero)
{
    vstack_ptr->as_sint = 0;
    ++vstack_ptr;
    ++ip;
    LAUF_INST_DISPATCH;
}
LAUF_INST_FN(push_small_zext)
{
    vstack_ptr->as_sint = ip->push_small_zext.literal;
    ++vstack_ptr;
    ++ip;
    LAUF_INST_DISPATCH;
}
LAUF_INST_FN(push_small_neg)
{
    vstack_ptr->as_sint = -lauf_value_sint(ip->push_small_zext.literal);
    ++vstack_ptr;
    ++ip;
    LAUF_INST_DISPATCH;
}

LAUF_INST_FN(local_addr)
{
    vstack_ptr->as_ptr = static_cast<unsigned char*>(frame_ptr) + ptrdiff_t(ip->local_addr.literal);
    ++vstack_ptr;
    ++ip;
    LAUF_INST_DISPATCH;
}
LAUF_INST_FN(array_element)
{
    auto idx       = vstack_ptr[-2].as_uint;
    auto elem_size = ptrdiff_t(ip->array_element.literal);
    auto addr      = vstack_ptr[-1].as_ptr;

    --vstack_ptr;
    vstack_ptr[-1].as_ptr = (unsigned char*)(addr) + idx * elem_size;

    ++ip;
    LAUF_INST_DISPATCH;
}

LAUF_INST_FN(drop)
{
    vstack_ptr -= ptrdiff_t(ip->drop.literal);
    ++ip;
    LAUF_INST_DISPATCH;
}
LAUF_INST_FN(pick)
{
    auto index  = ptrdiff_t(ip->pick.literal);
    auto value  = vstack_ptr[-index - 1];
    *vstack_ptr = value;
    ++vstack_ptr;
    ++ip;
    LAUF_INST_DISPATCH;
}
LAUF_INST_FN(dup)
{
    *vstack_ptr = vstack_ptr[-1];
    ++vstack_ptr;
    ++ip;
    LAUF_INST_DISPATCH;
}
LAUF_INST_FN(roll)
{
    auto index = ptrdiff_t(ip->pick.literal);
    auto value = vstack_ptr[-index - 1];
    std::memmove(vstack_ptr - index - 1, vstack_ptr - index, index * sizeof(lauf_value));
    vstack_ptr[-1] = value;
    ++ip;
    LAUF_INST_DISPATCH;
}
LAUF_INST_FN(swap)
{
    std::swap(vstack_ptr[-1], vstack_ptr[-2]);
    ++ip;
    LAUF_INST_DISPATCH;
}

LAUF_INST_FN(load_field)
{
    auto type = static_cast<lauf_type>(vm->mod->get_literal(ip->load_field.literal_idx).as_ptr);

    auto object    = vstack_ptr[-1].as_ptr;
    vstack_ptr[-1] = type->load_field(object, ip->load_field.field);

    ++ip;
    LAUF_INST_DISPATCH;
}
LAUF_INST_FN(store_field)
{
    auto type = static_cast<lauf_type>(vm->mod->get_literal(ip->store_field.literal_idx).as_ptr);

    auto object = const_cast<void*>(vstack_ptr[-1].as_ptr);
    type->store_field(object, ip->store_field.field, vstack_ptr[-2]);
    vstack_ptr -= 2;

    ++ip;
    LAUF_INST_DISPATCH;
}
LAUF_INST_FN(save_field)
{
    auto type = static_cast<lauf_type>(vm->mod->get_literal(ip->save_field.literal_idx).as_ptr);

    auto object = const_cast<void*>(vstack_ptr[-1].as_ptr);
    type->store_field(object, ip->save_field.field, vstack_ptr[-2]);
    vstack_ptr -= 1;

    ++ip;
    LAUF_INST_DISPATCH;
}
LAUF_INST_FN(load_value)
{
    auto object = static_cast<unsigned char*>(frame_ptr) + ptrdiff_t(ip->load_value.literal);

    *vstack_ptr = *reinterpret_cast<lauf_value*>(object);
    ++vstack_ptr;

    ++ip;
    LAUF_INST_DISPATCH;
}
LAUF_INST_FN(store_value)
{
    auto object = static_cast<unsigned char*>(frame_ptr) + ptrdiff_t(ip->store_value.literal);

    ::new (object) lauf_value(vstack_ptr[-1]);
    --vstack_ptr;

    ++ip;
    LAUF_INST_DISPATCH;
}
LAUF_INST_FN(save_value)
{
    auto object = static_cast<unsigned char*>(frame_ptr) + ptrdiff_t(ip->save_value.literal);

    ::new (object) lauf_value(vstack_ptr[-1]);

    ++ip;
    LAUF_INST_DISPATCH;
}

LAUF_INST_FN(panic)
{
    (void)ip;
    (void)frame_ptr;

    auto message = static_cast<const char*>(vstack_ptr[-1].as_ptr);
    vm->panic_handler(nullptr, message);
    return false;
}
LAUF_INST_FN(panic_if)
{
    auto value   = vstack_ptr[-2];
    auto message = static_cast<const char*>(vstack_ptr[-1].as_ptr);
    if (check_condition(ip->panic_if.cc, value))
    {
        vm->panic_handler(nullptr, message);
        return false;
    }

    vstack_ptr -= 2;
    ++ip;
    LAUF_INST_DISPATCH;
}

bool dispatch(bc_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr, lauf_vm vm)
{
    if (ip == nullptr)
        return true;

    inst_fn* fns[] = {
#define LAUF_BC_OP(Name, Data) &execute_##Name,
#include "lauf/detail/bc_ops.h"
#undef LAUF_BC_OP
    };

    [[clang::musttail]] return fns[size_t(ip->tag.op)](ip, vstack_ptr, frame_ptr, vm);
}
} // namespace

bool lauf_builtin_dispatch(void* ip, union lauf_value* stack_ptr, void* frame_ptr, void* vm)
{
    // The only difference between the builtin signature and the actual signature are void* vs typed
    // pointers. This does not matter for tail calls. However, clang will not generate tail calls
    // unless the signature matches exactly, so we tweak it a bit.
    auto fn = reinterpret_cast<lauf_builtin_function*>(&dispatch);
    [[clang::musttail]] return fn(ip, stack_ptr, frame_ptr, vm);
}

bool lauf_vm_execute(lauf_vm vm, lauf_program prog, const lauf_value* input, lauf_value* output)
{
    auto [mod, fn] = program(prog);
    vm->mod        = mod;

    auto vstack_ptr = vm->value_stack();
    std::memcpy(vstack_ptr, input, fn->input_count * sizeof(lauf_value));
    vstack_ptr += fn->input_count;

    vm->memory_stack.push(stack_frame{nullptr, vm->memory_stack.top(), nullptr});
    auto frame_ptr = vm->memory_stack.allocate(fn->local_stack_size, alignof(std::max_align_t));
    auto ip        = fn->bytecode();
    if (!dispatch(ip, vstack_ptr, frame_ptr, vm))
        return false;

    vstack_ptr -= fn->output_count;
    std::memcpy(output, vstack_ptr, fn->output_count * sizeof(lauf_value));

    vm->memory_stack.reset();
    return true;
}

