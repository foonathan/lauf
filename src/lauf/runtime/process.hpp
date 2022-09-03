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

    // When not running, nullptr.
    // When suspended, the suspend instruction.
    // When active, the resume instruction.
    const lauf_asm_inst*      ip         = nullptr;
    lauf_runtime_value*       vstack_ptr = nullptr;
    lauf_runtime_stack_frame* frame_ptr  = nullptr;
    // Only valid when active, fiber that resumed it.
    fiber* resumer = nullptr;

    // The very base of the stack frame.
    lauf_runtime_stack_frame trampoline_frame;

    // Intrusively linked list of fibers.
    fiber* prev_fiber = nullptr;
    fiber* next_fiber = nullptr;

    //=== operations ===//
    static fiber* create(lauf_runtime_process* process);
    static void   destroy(lauf_runtime_process* process, fiber* fiber);

    void init(const lauf_asm_function* fn)
    {
        trampoline_frame.function = fn;
    }
    void copy_output(lauf_runtime_value* output);

    //=== access ===//
    bool is_running() const
    {
        return ip != nullptr;
    }

    bool is_main_fiber() const
    {
        assert(is_running());
        // The main fiber does not have a resumer since it was started by the VM.
        return resumer == nullptr;
    }

    const lauf_asm_function* root_function() const
    {
        return trampoline_frame.function;
    }
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

    std::size_t remaining_steps = 0;

    static lauf_runtime_process create(lauf_vm* vm, const lauf_asm_program* program);
    static void                 destroy(lauf_runtime_process* process);
};

#endif // SRC_LAUF_RUNTIME_PROCESS_HPP_INCLUDED

