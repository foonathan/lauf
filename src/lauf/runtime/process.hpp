// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_RUNTIME_PROCESS_HPP_INCLUDED
#define SRC_LAUF_RUNTIME_PROCESS_HPP_INCLUDED

#include <lauf/runtime/process.h>

#include <lauf/runtime/memory.hpp>
#include <lauf/runtime/stack.hpp>

typedef struct lauf_vm lauf_vm;

namespace lauf
{
struct fiber
{
    // The handle to itself.
    lauf_runtime_address handle;
    // The stacks of the fiber.
    lauf::vstack vstack;
    lauf::cstack cstack;

    // When suspended, state at the suspend instruction.
    // When active, state at the resume instruction.
    const lauf_asm_inst*      ip         = nullptr;
    lauf_runtime_value*       vstack_ptr = nullptr;
    lauf_runtime_stack_frame* frame_ptr  = nullptr;
    // Only valid when active, fiber that resumed it.
    fiber* resumer = nullptr;

    // The very base of the stack frame.
    lauf_runtime_stack_frame trampoline_frame;
    // Intrusively linked list of fibers.
    fiber* next_fiber = nullptr;

    static fiber* create(page_allocator& alloc, lauf_runtime_process* process,
                         const lauf_asm_function* fn, std::size_t initial_vstack_size,
                         std::size_t initial_cstack_size);

    static void destroy(page_allocator& alloc, lauf_runtime_process* process, fiber* fiber);
};
} // namespace lauf

struct lauf_runtime_process
{
    // The VM that is executing the process.
    lauf_vm* vm = nullptr;

    lauf::fiber* cur_fiber = nullptr;
    lauf::memory memory;
    lauf::fiber* fiber_list = nullptr;

    // The dummy frame for call stacks -- this is only lazily updated
    // It needs to be valid when calling a builtin or panicing.
    lauf_runtime_stack_frame callstack_leaf_frame;

    // The program that is running.
    const lauf_asm_program* program = nullptr;

    std::size_t remaining_steps;
};

inline lauf::fiber* lauf::fiber::create(page_allocator& alloc, lauf_runtime_process* process,
                                        const lauf_asm_function* fn,
                                        std::size_t              initial_vstack_size,
                                        std::size_t              initial_cstack_size)
{
    lauf::cstack stack;
    stack.init(alloc, initial_cstack_size);

    auto fiber = ::new (stack.base()) lauf::fiber();

    fiber->handle = process->memory.new_allocation(alloc, make_fiber_alloc(fiber));
    fiber->vstack.init(alloc, initial_vstack_size);
    fiber->cstack = stack;

    fiber->trampoline_frame.function = fn;
    fiber->trampoline_frame.next_offset
        = sizeof(lauf::fiber) - offsetof(lauf::fiber, trampoline_frame);

    fiber->next_fiber   = process->fiber_list;
    process->fiber_list = fiber;

    return fiber;
}

inline void lauf::fiber::destroy(page_allocator& alloc, lauf_runtime_process* process, fiber* fiber)
{
    process->memory[fiber->handle.allocation].status = lauf::allocation_status::freed;
    process->fiber_list                              = fiber->next_fiber;

    fiber->vstack.clear(alloc);
    fiber->cstack.clear(alloc);
}

#endif // SRC_LAUF_RUNTIME_PROCESS_HPP_INCLUDED

