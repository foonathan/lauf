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
    const lauf_asm_inst*      ip;
    lauf_runtime_value*       vstack_ptr;
    lauf_runtime_stack_frame* frame_ptr;

    // Only valid when active, fiber that resumed it.
    fiber* resumer;

    static fiber* create(page_allocator& alloc, lauf_runtime_process* process,
                         std::size_t initial_vstack_size, std::size_t initial_cstack_size);

    void destroy(page_allocator& alloc, lauf_runtime_process* process);
};
} // namespace lauf

struct lauf_runtime_process
{
    // The VM that is executing the process.
    lauf_vm* vm = nullptr;

    lauf::fiber*             cur_fiber;
    lauf::memory             memory;
    lauf::array<lauf::fiber> fibers;

    // The dummy frame for call stacks -- this is only lazily updated
    // It needs to be valid when calling a builtin or panicing.
    lauf_runtime_stack_frame callstack_leaf_frame;

    // The program that is running.
    const lauf_asm_program* program = nullptr;

    std::size_t remaining_steps;
};

inline lauf::fiber* lauf::fiber::create(page_allocator& alloc, lauf_runtime_process* process,
                                        std::size_t initial_vstack_size,
                                        std::size_t initial_cstack_size)
{
    auto fiber = &process->fibers.emplace_back(alloc);
    fiber->vstack.init(alloc, initial_vstack_size);
    fiber->cstack.init(alloc, initial_cstack_size);

    fiber->handle = process->memory.new_allocation(alloc, make_fiber_alloc(fiber));

    return fiber;
}

inline void lauf::fiber::destroy(page_allocator& alloc, lauf_runtime_process* process)
{
    process->memory[handle.allocation].status = lauf::allocation_status::freed;
    vstack.clear(alloc);
    cstack.clear(alloc);
}

#endif // SRC_LAUF_RUNTIME_PROCESS_HPP_INCLUDED

