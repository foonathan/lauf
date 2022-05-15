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
    lauf_vm_instruction*    return_ip;
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

#if LAUF_HAS_TAIL_CALL
// Each instruction is a function that tail calls the next one.

namespace
{
bool dispatch(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr, lauf_vm vm);

#    define LAUF_BC_OP(Name, Data, ...)                                                            \
        bool execute_##Name(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr,      \
                            lauf_vm vm) __VA_ARGS__
#    define LAUF_DISPATCH LAUF_TAIL_CALL return dispatch(ip, vstack_ptr, frame_ptr, vm)
#    define LAUF_DISPATCH_BUILTIN(Callee, StackChange)                                             \
        LAUF_TAIL_CALL return Callee(ip, vstack_ptr, frame_ptr, vm)

#    include "lauf/detail/bc_ops.h"

#    undef LAUF_BC_OP
#    undef LAUF_DISPATCH
#    undef LAUF_DISPATCH_BUILTIN

constexpr lauf_builtin_function* inst_fns[] = {
#    define LAUF_BC_OP(Name, Data, ...) &execute_##Name,
#    include "lauf/detail/bc_ops.h"
#    undef LAUF_BC_OP
};

bool dispatch(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr, lauf_vm vm)
{
    if (ip == nullptr)
        return true;
    LAUF_TAIL_CALL return inst_fns[size_t(ip->tag.op)](ip, vstack_ptr, frame_ptr, vm);
}
} // namespace

bool lauf_builtin_dispatch(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr,
                           lauf_vm vm)
{
    // ip can't be null after a builtin as it can't return.
    LAUF_TAIL_CALL return inst_fns[size_t(ip->tag.op)](ip, vstack_ptr, frame_ptr, vm);
}

#elif LAUF_HAS_COMPUTED_GOTO
// Use computed goto for dispatch.

namespace
{
bool dispatch(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr, lauf_vm vm)
{
    void* labels[] = {
#    define LAUF_BC_OP(Name, Data, ...) &&execute_##Name,
#    include "lauf/detail/bc_ops.h"
#    undef LAUF_BC_OP
    };

    goto* labels[size_t(ip->tag.op)];

#    define LAUF_BC_OP(Name, Data, ...) execute_##Name : __VA_ARGS__
#    define LAUF_DISPATCH                                                                          \
        do                                                                                         \
        {                                                                                          \
            if (ip == nullptr)                                                                     \
                return true;                                                                       \
            goto* labels[size_t(ip->tag.op)];                                                      \
        } while (0)
#    define LAUF_DISPATCH_BUILTIN(Callee, StackChange)                                             \
        do                                                                                         \
        {                                                                                          \
            Callee(ip, vstack_ptr, frame_ptr, vm);                                                 \
            vstack_ptr += (StackChange);                                                           \
            goto* labels[size_t(ip->tag.op)];                                                      \
        } while (0);

#    include "lauf/detail/bc_ops.h"

#    undef LAUF_BC_OP
#    undef LAUF_DISPATCH
#    undef LAUF_DISPATCH_BUILTIN
}
} // namespace

bool lauf_builtin_dispatch(lauf_vm_instruction*, lauf_value*, void*, lauf_vm)
{
    // We terminate the call here.
    return true;
}

#else
// Simple switch statement in a loop.

namespace
{
bool dispatch(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr, lauf_vm vm)
{
    while (ip != nullptr)
    {
        switch (ip->tag.op)
        {
#    define LAUF_BC_OP(Name, Data, ...)                                                            \
    case bc_op::Name:                                                                              \
        __VA_ARGS__
#    define LAUF_DISPATCH break
#    define LAUF_DISPATCH_BUILTIN(Callee, StackChange)                                             \
        {                                                                                          \
            Callee(ip, vstack_ptr, frame_ptr, vm);                                                 \
            vstack_ptr += (StackChange);                                                           \
            break
        }

#    include "lauf/detail/bc_ops.h"

#    undef LAUF_BC_OP
#    undef LAUF_DISPATCH
#    undef LAUF_DISPATCH_FN
    }
}

return true;
} // namespace
} // namespace

bool lauf_builtin_dispatch(lauf_vm_instruction*, lauf_value*, void*, lauf_vm)
{
    // We terminate the call here.
    return true;
}

#endif

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

