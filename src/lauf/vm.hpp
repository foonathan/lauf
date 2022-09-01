// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_VM_HPP_INCLUDED
#define SRC_LAUF_VM_HPP_INCLUDED

#include <lauf/vm.h>

#include <lauf/asm/instruction.hpp>
#include <lauf/runtime/process.hpp>
#include <lauf/runtime/value.h>
#include <lauf/support/arena.hpp>
#include <lauf/support/page_allocator.hpp>

struct lauf_vm : lauf::intrinsic_arena<lauf_vm>
{
    lauf_vm_panic_handler panic_handler;
    lauf_vm_allocator     heap_allocator;
    lauf::page_allocator  page_allocator;

    // In number of elements.
    std::size_t initial_vstack_size;
    std::size_t max_vstack_size;
    // In number of bytes.
    std::size_t initial_cstack_size;
    std::size_t max_cstack_size;

    std::size_t step_limit;

    explicit lauf_vm(lauf::arena_key key, lauf_vm_options options)
    : lauf::intrinsic_arena<lauf_vm>(key), panic_handler(options.panic_handler),
      heap_allocator(options.allocator),
      initial_vstack_size(options.initial_vstack_size_in_elements),
      max_vstack_size(options.max_vstack_size_in_elements),
      initial_cstack_size(options.initial_cstack_size_in_bytes),
      max_cstack_size(options.max_cstack_size_in_bytes), step_limit(options.step_limit)
    {}

    ~lauf_vm()
    {
        page_allocator.release();
    }
};

#endif // SRC_LAUF_VM_HPP_INCLUDED

