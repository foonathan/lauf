// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_IMPL_VM_HPP_INCLUDED
#define SRC_LAUF_IMPL_VM_HPP_INCLUDED

#include <lauf/detail/stack_allocator.hpp>
#include <lauf/vm.h>

struct alignas(lauf_value) lauf_vm_impl
{
    lauf_vm_process             process;
    lauf_panic_handler          panic_handler;
    size_t                      value_stack_size;
    lauf::_detail::memory_stack memory_stack;

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

namespace lauf::_detail
{
size_t frame_size_for(lauf_function fn);
} // namespace lauf::_detail

#endif // SRC_LAUF_IMPL_VM_HPP_INCLUDED

