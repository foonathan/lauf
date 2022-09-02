// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_RUNTIME_PROCESS_HPP_INCLUDED
#define SRC_LAUF_RUNTIME_PROCESS_HPP_INCLUDED

#include <lauf/runtime/process.h>

#include <lauf/runtime/memory.hpp>
#include <lauf/runtime/stack.hpp>
#include <lauf/support/array.hpp>

typedef struct lauf_asm_function lauf_asm_function;
typedef union lauf_asm_inst      lauf_asm_inst;
typedef struct lauf_asm_global   lauf_asm_global;
typedef struct lauf_vm           lauf_vm;

struct lauf_runtime_process
{
    // The VM that is executing the process.
    lauf_vm* vm = nullptr;

    lauf::vstack vstack;
    lauf::cstack cstack;

    // The dummy frame for call stacks -- this is only lazily updated
    // It needs to be valid when calling a builtin or panicing.
    lauf_runtime_stack_frame callstack_leaf_frame;

    // The program that is running.
    const lauf_asm_program* program = nullptr;

    // The allocations of the process.
    // They are allocated using the page_allocator of the VM.
    lauf::array<lauf::allocation> allocations;
    std::uint8_t                  alloc_generation = 0;

    std::size_t remaining_steps;

    lauf_runtime_address add_allocation(lauf::allocation alloc);

    lauf::allocation* get_allocation(lauf_runtime_address addr)
    {
        if (LAUF_UNLIKELY(addr.allocation >= allocations.size()))
            return nullptr;

        auto alloc = &allocations[addr.allocation];
        if (LAUF_UNLIKELY((alloc->generation & 0b11) != addr.generation))
            return nullptr;

        return alloc;
    }

    // This function is called on frame entry of functions with local variables.
    // It will garbage collect allocations that have been freed.
    void try_free_allocations()
    {
        if (allocations.empty() || allocations.back().status != lauf::allocation_status::freed)
            // We only remove from the back.
            return;

        do
        {
            allocations.pop_back();
        } while (!allocations.empty()
                 && allocations.back().status == lauf::allocation_status::freed);

        // Since we changed something, we need to increment the generation.
        ++alloc_generation;
    }
};

#endif // SRC_LAUF_RUNTIME_PROCESS_HPP_INCLUDED

