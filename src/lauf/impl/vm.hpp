// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_IMPL_VM_HPP_INCLUDED
#define SRC_LAUF_IMPL_VM_HPP_INCLUDED

#include <lauf/jit.h>
#include <lauf/support/stack_allocator.hpp>
#include <lauf/vm.h>

struct alignas(lauf_value) lauf_vm_impl
{
    lauf_vm_process    process;
    lauf_panic_handler panic_handler;
    lauf_vm_allocator  allocator;
    size_t             value_stack_size;
    lauf::memory_stack memory_stack;
    lauf_jit_compiler  jit;

    lauf_vm_impl(lauf_vm_options options);

    lauf_vm_impl(const lauf_vm_impl&) = delete;
    lauf_vm_impl& operator=(const lauf_vm_impl&) = delete;

    ~lauf_vm_impl();

    lauf_value* value_stack()
    {
        return reinterpret_cast<lauf_value*>(this + 1) + value_stack_size;
    }
    lauf_value* value_stack_limit()
    {
        return reinterpret_cast<lauf_value*>(this + 1);
    }
};

namespace lauf
{
size_t frame_size_for(lauf_function fn);

bool dispatch(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr,
              lauf_vm_process process);

bool jit_finish(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr,
                lauf_vm_process process);
} // namespace lauf

#endif // SRC_LAUF_IMPL_VM_HPP_INCLUDED

