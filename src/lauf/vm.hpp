// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_VM_HPP_INCLUDED
#define SRC_LAUF_VM_HPP_INCLUDED

#include <lauf/vm.h>

#include <lauf/asm/instruction.hpp>
#include <lauf/runtime/process.hpp>
#include <lauf/runtime/value.h>
#include <lauf/support/arena.hpp>

struct lauf_vm : lauf::intrinsic_arena<lauf_vm>
{
    lauf_vm_panic_handler panic_handler;
    lauf_vm_allocator     allocator;

    // Grows down.
    lauf_runtime_value* vstack_base;
    std::size_t         vstack_size;

    lauf::stack_chunk* cstack;
    std::size_t        max_cstack_chunks;

    std::size_t step_limit;

    explicit lauf_vm(lauf::arena_key key, lauf_vm_options options)
    : lauf::intrinsic_arena<lauf_vm>(key), panic_handler(options.panic_handler),
      allocator(options.allocator), vstack_size(options.vstack_size_in_elements),
      max_cstack_chunks(options.max_cstack_size_in_bytes / sizeof(lauf::stack_chunk)),
      step_limit(options.step_limit)
    {
        // We allocate the stacks using new, as unlike the arena, their memory should not be freed
        // between executions.

        cstack = lauf::stack_chunk::allocate();
        if (options.initial_cstack_size_in_bytes > sizeof(lauf::stack_chunk))
        {
            auto cur = cstack;
            do
            {
                options.initial_cstack_size_in_bytes -= sizeof(lauf::stack_chunk);
                cur->next = lauf::stack_chunk::allocate();
                cur       = cur->next;
            } while (options.initial_cstack_size_in_bytes > sizeof(lauf::stack_chunk));
        }

        // It grows down, so the base is at the end.
        vstack_base = static_cast<lauf_runtime_value*>(
                          ::operator new(vstack_size * sizeof(lauf_runtime_value)))
                      + vstack_size;
    }

    ~lauf_vm()
    {
        for (auto cur = cstack; cur != nullptr;)
            cur = lauf::stack_chunk::free(cur);

        ::operator delete(vstack_base - vstack_size);
    }

    lauf_runtime_value* vstack_end() const
    {
        // We keep a buffer of UINT8_MAX.
        // This ensures that we can always call a builtin, which can push at most that many values.
        return vstack_base - vstack_size + UINT8_MAX;
    }
};

#endif // SRC_LAUF_VM_HPP_INCLUDED

