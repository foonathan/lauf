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

using namespace lauf::_detail;

struct alignas(lauf_value) lauf_vm_impl
{
    struct
    {
        const lauf_value* literals;
        lauf_function*    functions;
    } state;
    lauf_panic_handler panic_handler;
    size_t             value_stack_size;
    stack_allocator    memory_stack;

    lauf_value* value_stack()
    {
        return reinterpret_cast<lauf_value*>(this + 1) + value_stack_size;
    }

    lauf_value get_literal(bc_literal_idx idx) const
    {
        return state.literals[size_t(idx)];
    }
    lauf_function get_function(bc_function_idx idx) const
    {
        return state.functions[size_t(idx)];
    }
};

namespace
{
struct alignas(std::max_align_t) stack_frame
{
    lauf_function           fn;
    lauf_vm_instruction*    return_ip;
    stack_allocator::marker unwind;
    stack_frame*            prev;
};

void* new_stack_frame(lauf_vm vm, void* frame_ptr, lauf_vm_instruction* return_ip, lauf_function fn)
{
    auto marker     = vm->memory_stack.top();
    auto prev_frame = static_cast<stack_frame*>(frame_ptr) - 1;

    // As the local_stack_size is a multiple of max alignment, we don't need to worry about aligning
    // it; the builder takes care of it when computing the stack size.
    auto memory = vm->memory_stack.allocate(sizeof(stack_frame) + fn->local_stack_size);
    auto frame  = ::new (memory) stack_frame{fn, return_ip, marker, prev_frame};
    return frame + 1;
}

} // namespace

//=== backtrace ===//
// lauf_backtrace is a pointer to a stack frame.
// It is initially a pointer to a dummy stack frame whose return_ip stores the initial ip + 1,
// and prev->fn is the current function.

lauf_function lauf_backtrace_get_function(lauf_backtrace bt)
{
    auto frame = static_cast<stack_frame*>(bt);
    return frame->prev->fn;
}

lauf_debug_location lauf_backtrace_get_location(lauf_backtrace bt)
{
    auto frame = static_cast<stack_frame*>(bt);
    return lauf_function_get_location_of(frame->prev->fn, frame->return_ip - 1);
}

lauf_backtrace lauf_backtrace_parent(lauf_backtrace bt)
{
    auto frame = static_cast<stack_frame*>(bt);
    // We need to go to the parent.
    auto next_bt = frame->prev;

    if (next_bt->prev == nullptr)
    {
        // The parent of the next frame has no parent.
        // This means we've reached the top frame.
        assert(next_bt->return_ip == nullptr);
        return nullptr;
    }

    return next_bt;
}

//=== panic handler ===//
struct lauf_panic_info_impl
{
    stack_frame fake_frame;
};

namespace
{
lauf_panic_info_impl make_panic_info(void* frame_ptr, lauf_vm_instruction* ip)
{
    auto cur_frame = static_cast<stack_frame*>(frame_ptr) - 1;
    return {{nullptr, ip + 1, {}, cur_frame}};
}
} // namespace

lauf_backtrace lauf_panic_info_get_backtrace(lauf_panic_info info)
{
    return &info->fake_frame;
}

//=== vm functions ===//
const lauf_vm_options lauf_default_vm_options
    = {size_t(512) * 1024, [](lauf_panic_info info, const char* message) {
           std::fprintf(stderr, "[lauf] panic: %s\n", message);
           std::fprintf(stderr, "stack backtrace\n");

           auto index = 0;
           for (auto cur = lauf_panic_info_get_backtrace(info); cur != nullptr;
                cur      = lauf_backtrace_parent(cur))
           {
               auto fn = lauf_backtrace_get_function(cur);
               std::fprintf(stderr, " %4d. %s\n", index, lauf_function_get_name(fn));
               if (auto loc = lauf_backtrace_get_location(cur); loc.line != 0u && loc.column != 0u)
               {
                   auto path = lauf_module_get_path(lauf_function_get_module(fn));
                   std::fprintf(stderr, "        at %s:%u:%u\n", path, unsigned(loc.line),
                                unsigned(loc.column));
               }
               ++index;
           }
       }};

lauf_vm lauf_vm_create(lauf_vm_options options)
{
    auto memory = ::operator new(sizeof(lauf_vm_impl) + options.max_value_stack_size);
    return ::new (memory)
        lauf_vm_impl{{}, options.panic_handler, options.max_value_stack_size / sizeof(lauf_value)};
}

void lauf_vm_destroy(lauf_vm vm)
{
    ::operator delete(vm);
}

//=== execute ===//
namespace
{
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
    vm->state      = {mod->literal_data(), mod->function_begin()};

    auto vstack_ptr = vm->value_stack();

    for (auto i = 0; i != fn->input_count; ++i)
    {
        --vstack_ptr;
        vstack_ptr[0] = input[i];
    }

    auto frame_ptr                                = new_stack_frame(vm, nullptr, nullptr, fn);
    static_cast<stack_frame*>(frame_ptr)[-1].prev = nullptr;

    auto ip = fn->bytecode();
    if (!dispatch(ip, vstack_ptr, frame_ptr, vm))
        return false;

    for (auto i = 0; i != fn->output_count; ++i)
    {
        output[i] = vstack_ptr[0];
        ++vstack_ptr;
    }

    vm->memory_stack.reset();
    return true;
}

