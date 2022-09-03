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
struct registers
{
    const lauf_asm_inst*      ip;
    lauf_runtime_value*       vstack_ptr;
    lauf_runtime_stack_frame* frame_ptr;
};
} // namespace lauf

struct lauf_runtime_fiber
{
    // The state of the fiber.
    enum state_t : uint8_t
    {
        done,
        ready,
        suspended,
        running,
    } state = done;

    // The handle to itself.
    uint32_t handle_allocation : 30;
    uint32_t handle_generation : 2;

    // The stacks of the fiber.
    lauf::vstack vstack;
    lauf::cstack cstack;

    union
    {
        // Only when suspended.
        lauf::registers suspension_point;
        // Only when running.
        lauf_runtime_address resumer;
    };

    // The very base of the stack frame.
    lauf_runtime_stack_frame trampoline_frame;

    // Intrusively linked list of fibers.
    lauf_runtime_fiber* prev_fiber = nullptr;
    lauf_runtime_fiber* next_fiber = nullptr;

    //=== operations ===//
    static lauf_runtime_fiber* create(lauf_runtime_process* process);
    static void                destroy(lauf_runtime_process* process, lauf_runtime_fiber* fiber);

    void init(const lauf_asm_function* fn)
    {
        assert(state == done);
        trampoline_frame.function = fn;
        state                     = ready;
    }
    void copy_output(lauf_runtime_value* output);

    void suspend(lauf::registers regs)
    {
        assert(state == running);
        state            = suspended;
        suspension_point = regs;
    }

    lauf::registers resume_by(lauf_runtime_fiber* resumer)
    {
        assert(state == suspended || state == ready);
        auto regs = suspension_point;

        state         = running;
        this->resumer = resumer == nullptr ? lauf_runtime_address_null : resumer->handle();

        return regs;
    }

    //=== access ===//
    const lauf_asm_function* root_function() const
    {
        return trampoline_frame.function;
    }

    bool has_resumer() const
    {
        assert(state == running);
        return resumer.allocation != lauf_runtime_address_null.allocation;
    }

    lauf_runtime_address handle() const
    {
        return {handle_allocation, handle_generation, 0};
    }
};

struct lauf_runtime_process
{
    // The VM that is executing the process.
    lauf_vm* vm = nullptr;

    lauf_runtime_fiber* cur_fiber = nullptr;
    lauf::memory        memory;
    lauf_runtime_fiber* fiber_list = nullptr;

    // The state of the current fiber, not set when all fibers suspended.
    // Only lazily updated whenever process is exposed to user code:
    // * before calling a builtin
    // * before panicing
    lauf::registers regs;
    // The program that is running.
    const lauf_asm_program* program = nullptr;

    std::size_t remaining_steps = 0;

    static lauf_runtime_process create(lauf_vm* vm, const lauf_asm_program* program);
    static void                 destroy(lauf_runtime_process* process);
};

namespace lauf
{
inline lauf_runtime_fiber* get_fiber(lauf_runtime_process* process, lauf_runtime_address handle)
{
    auto alloc = process->memory.try_get(handle);
    if (LAUF_UNLIKELY(alloc == nullptr || alloc->source != lauf::allocation_source::fiber_memory))
        return nullptr;
    return static_cast<lauf_runtime_fiber*>(alloc->ptr);
}
} // namespace lauf

#endif // SRC_LAUF_RUNTIME_PROCESS_HPP_INCLUDED

