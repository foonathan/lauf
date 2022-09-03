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
    const lauf_asm_inst*      ip         = nullptr;
    lauf_runtime_value*       vstack_ptr = nullptr;
    lauf_runtime_stack_frame* frame_ptr  = nullptr;
};
} // namespace lauf

struct lauf_runtime_fiber
{
    // The handle to itself.
    lauf_runtime_address handle;
    // The stacks of the fiber.
    lauf::vstack vstack;
    lauf::cstack cstack;

    // When not running, nullptr.
    // When suspended, the suspend instruction.
    lauf::registers regs;
    // When active, the resume instruction.
    // Only valid when active, fiber that resumed it.
    lauf_runtime_fiber* resumer = nullptr;

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
        trampoline_frame.function = fn;
    }
    void copy_output(lauf_runtime_value* output);

    //=== access ===//
    bool is_running() const
    {
        return regs.ip != nullptr;
    }

    const lauf_asm_function* root_function() const
    {
        return trampoline_frame.function;
    }
};

struct lauf_runtime_process
{
    // The VM that is executing the process.
    lauf_vm* vm = nullptr;

    lauf_runtime_fiber* cur_fiber = nullptr;
    lauf::memory        memory;
    lauf_runtime_fiber* fiber_list = nullptr;

    // The current instruction pointer.
    // Only lazily updated whenever process is exposed to user code:
    // * before calling a builtin
    // * before panicing
    // * before suspending the main fiber.
    lauf::registers regs;
    // The program that is running.
    const lauf_asm_program* program = nullptr;

    std::size_t remaining_steps = 0;

    static lauf_runtime_process create(lauf_vm* vm, const lauf_asm_program* program);
    static void                 destroy(lauf_runtime_process* process);
};

#endif // SRC_LAUF_RUNTIME_PROCESS_HPP_INCLUDED

