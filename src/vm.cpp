// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/vm.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <lauf/builtin.h>
#include <lauf/bytecode.hpp>
#include <lauf/impl/module.hpp>
#include <lauf/impl/process.hpp>
#include <lauf/impl/program.hpp>
#include <lauf/impl/vm.hpp>
#include <lauf/support/stack_allocator.hpp>
#include <lauf/type.h>

namespace
{
struct alignas(void*) stack_frame
{
    lauf_function                 fn;
    lauf_vm_instruction*          return_ip;
    lauf::stack_allocator::marker unwind;
    lauf_value_address            first_local_allocation;
    stack_frame*                  prev;
};
} // namespace

std::size_t lauf::frame_size_for(lauf_function fn)
{
    return sizeof(stack_frame) + fn->local_stack_size;
}

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

    if (next_bt->prev->fn == nullptr)
        return nullptr;
    else
        return next_bt;
}

//=== panic handler ===//
struct lauf_panic_info_impl
{
    stack_frame fake_frame;
};

lauf_backtrace lauf_panic_info_get_backtrace(lauf_panic_info info)
{
    return &info->fake_frame;
}

//=== allocator ===//
const lauf_vm_allocator lauf_vm_null_allocator
    = {nullptr, [](void*, size_t, size_t) -> void* { return nullptr; }, [](void*, void*) {}};

const lauf_vm_allocator lauf_vm_malloc_allocator
    = {nullptr,
       [](void*, size_t size, size_t alignment) {
           return alignment > alignof(std::max_align_t) ? nullptr : std::calloc(size, 1);
       },
       [](void*, void* memory) { std::free(memory); }};

//=== vm functions ===//
lauf_vm_impl::lauf_vm_impl(lauf_vm_options options)
: process(lauf_vm_process_impl::create_null(this)), panic_handler(options.panic_handler),
  allocator(options.allocator), value_stack_size(options.max_value_stack_size / sizeof(lauf_value)),
  memory_stack(options.max_stack_size)
{}

lauf_vm_impl::~lauf_vm_impl()
{
    lauf_vm_process_impl::destroy(process);
}
const lauf_vm_options lauf_default_vm_options
    = {size_t(128) * 1024, size_t(896) * 1024,
       // Panic handler prints message and backtrace.
       [](lauf_panic_info info, const char* message) {
           std::fprintf(stderr, "[lauf] panic: %s\n",
                        message == nullptr ? "(invalid message address)" : message);
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
       },
       lauf_vm_null_allocator};

lauf_vm lauf_vm_create(lauf_vm_options options)
{
    auto memory = ::operator new(sizeof(lauf_vm_impl) + options.max_value_stack_size);
    return ::new (memory) lauf_vm_impl(options);
}

void lauf_vm_destroy(lauf_vm vm)
{
    vm->~lauf_vm_impl();
    ::operator delete(vm);
}

void lauf_vm_set_panic_handler(lauf_vm vm, lauf_panic_handler handler)
{
    vm->panic_handler = handler;
}

//=== execute ===//
namespace
{
LAUF_NOINLINE_IF_TAIL bool do_panic(lauf_vm_instruction* ip, lauf_value* vstack_ptr,
                                    void* frame_ptr, lauf_vm_process process)
{
    auto cur_frame = static_cast<stack_frame*>(frame_ptr) - 1;
    auto info      = lauf_panic_info_impl{{nullptr, ip + 1, {}, lauf_value_address{}, cur_frame}};
    auto message   = static_cast<const char*>(vstack_ptr->as_native_ptr);
    process->vm()->panic_handler(&info, message);
    return false;
}

#define LAUF_DO_PANIC(Msg)                                                                         \
    do                                                                                             \
    {                                                                                              \
        vstack_ptr->as_native_ptr = (Msg);                                                         \
        LAUF_TAIL_CALL return do_panic(ip, vstack_ptr, frame_ptr, process);                        \
    } while (0)

LAUF_NOINLINE_IF_TAIL bool resize_allocation_list(lauf_vm_instruction* ip, lauf_value* vstack_ptr,
                                                  void* frame_ptr, lauf_vm_process process)
{
    // We resize the allocation list.
    lauf_vm_process_impl::resize_allocation_list(process);
    // And try executing the same instruction again.
    LAUF_TAIL_CALL return lauf_builtin_dispatch(ip, vstack_ptr, frame_ptr, process);
}

LAUF_NOINLINE_IF_TAIL bool reserve_new_stack_block(lauf_vm_instruction* ip, lauf_value* vstack_ptr,
                                                   void* frame_ptr, lauf_vm_process process)
{
    // We reserve a new stack block.
    if (!process->stack().reserve_new_block())
        LAUF_DO_PANIC("stack overflow");

    // And try executing the same instruction again.
    LAUF_TAIL_CALL return lauf_builtin_dispatch(ip, vstack_ptr, frame_ptr, process);
}

LAUF_INLINE bool check_condition(lauf::condition_code cc, lauf_value value)
{
    switch (cc)
    {
    case lauf::condition_code::is_zero:
        return value.as_sint == 0;
    case lauf::condition_code::is_nonzero:
        return value.as_sint != 0;
    case lauf::condition_code::cmp_lt:
        return value.as_sint < 0;
    case lauf::condition_code::cmp_le:
        return value.as_sint <= 0;
    case lauf::condition_code::cmp_gt:
        return value.as_sint > 0;
    case lauf::condition_code::cmp_ge:
        return value.as_sint >= 0;
    }
}
} // namespace

#if LAUF_HAS_TAIL_CALL
// Each instruction is a function that tail calls the next one.

namespace
{
LAUF_INLINE bool dispatch(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr,
                          lauf_vm_process process);

#    define LAUF_BC_OP(Name, Data, ...)                                                            \
        bool execute_##Name(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr,      \
                            lauf_vm_process process) __VA_ARGS__
#    define LAUF_DISPATCH LAUF_TAIL_CALL return dispatch(ip, vstack_ptr, frame_ptr, process)
#    define LAUF_DISPATCH_BUILTIN(Callee, StackChange)                                             \
        LAUF_TAIL_CALL return Callee(ip, vstack_ptr, frame_ptr, process)

#    include "lauf/bc_ops.h"

#    undef LAUF_BC_OP
#    undef LAUF_DISPATCH
#    undef LAUF_DISPATCH_BUILTIN

constexpr lauf_builtin_function* inst_fns[] = {
#    define LAUF_BC_OP(Name, Data, ...) &execute_##Name,
#    include "lauf/bc_ops.h"
#    undef LAUF_BC_OP
};

LAUF_INLINE bool dispatch(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr,
                          lauf_vm_process process)
{
    LAUF_TAIL_CALL return inst_fns[size_t(ip->tag.op)](ip, vstack_ptr, frame_ptr, process);
}
} // namespace

bool lauf_builtin_dispatch(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr,
                           lauf_vm_process process)
{
    LAUF_TAIL_CALL return inst_fns[size_t(ip->tag.op)](ip, vstack_ptr, frame_ptr, process);
}

#elif LAUF_HAS_COMPUTED_GOTO
// Use computed goto for dispatch.

namespace
{
bool dispatch(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr,
              lauf_vm_process process)
{
    void* labels[] = {
#    define LAUF_BC_OP(Name, Data, ...) &&execute_##Name,
#    include "lauf/bc_ops.h"
#    undef LAUF_BC_OP
    };

    goto* labels[size_t(ip->tag.op)];

#    define LAUF_BC_OP(Name, Data, ...) execute_##Name : __VA_ARGS__
#    define LAUF_DISPATCH                                                                          \
        do                                                                                         \
        {                                                                                          \
            goto* labels[size_t(ip->tag.op)];                                                      \
        } while (0)
#    define LAUF_DISPATCH_BUILTIN(Callee, StackChange)                                             \
        do                                                                                         \
        {                                                                                          \
            Callee(ip, vstack_ptr, frame_ptr, process);                                            \
            vstack_ptr += (StackChange);                                                           \
            goto* labels[size_t(ip->tag.op)];                                                      \
        } while (0);

#    include "lauf/bc_ops.h"

#    undef LAUF_BC_OP
#    undef LAUF_DISPATCH
#    undef LAUF_DISPATCH_BUILTIN
}
} // namespace

bool lauf_builtin_dispatch(lauf_vm_instruction*, lauf_value*, void*, lauf_vm_process)
{
    // We terminate the call here.
    return true;
}

#else
// Simple switch statement in a loop.

namespace
{
bool dispatch(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr,
              lauf_vm_process process)
{
    while (true)
    {
        switch (ip->tag.op)
        {
#    define LAUF_BC_OP(Name, Data, ...)                                                            \
    case bc_op::Name:                                                                              \
        __VA_ARGS__
#    define LAUF_DISPATCH break
#    define LAUF_DISPATCH_BUILTIN(Callee, StackChange)                                             \
        {                                                                                          \
            Callee(ip, vstack_ptr, frame_ptr, process);                                            \
            vstack_ptr += (StackChange);                                                           \
            break
        }

#    include "lauf/bc_ops.h"

#    undef LAUF_BC_OP
#    undef LAUF_DISPATCH
#    undef LAUF_DISPATCH_FN
    }
}

return true;
} // namespace
} // namespace

bool lauf_builtin_dispatch(lauf_vm_instruction*, lauf_value*, void*, lauf_vm_process)
{
    // We terminate the call here.
    return true;
}

#endif

bool lauf_builtin_panic(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr,
                        lauf_vm_process process)
{
    // call_builtin has already incremented ip, so undo it to get the location.
    LAUF_TAIL_CALL return do_panic(ip - 1, vstack_ptr, frame_ptr, process);
}

bool lauf_vm_execute(lauf_vm vm, lauf_program prog, const lauf_value* input, lauf_value* output)
{
    vm->process->start(prog);
    auto vstack_ptr = vm->value_stack();

    for (auto i = 0; i != prog.entry->input_count; ++i)
    {
        --vstack_ptr;
        vstack_ptr[0] = input[i];
    }

    lauf_vm_instruction startup[]
        = {LAUF_VM_INSTRUCTION(call, lauf::bc_function_idx(prog.mod->find_function(prog.entry))),
           LAUF_VM_INSTRUCTION(exit)};

    stack_frame frame{};
    auto        result = dispatch(startup, vstack_ptr, &frame + 1, vm->process);
    if (result)
    {
        vstack_ptr = vm->value_stack() - prog.entry->output_count;
        for (auto i = 0; i != prog.entry->output_count; ++i)
        {
            output[i] = vstack_ptr[0];
            ++vstack_ptr;
        }
    }

    vm->process->finish();
    vm->memory_stack.reset();
    return result;
}

