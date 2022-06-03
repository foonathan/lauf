// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_IMPL_PROCESS_HPP_INCLUDED
#define SRC_LAUF_IMPL_PROCESS_HPP_INCLUDED

#include <cassert>
#include <lauf/impl/module.hpp>
#include <lauf/impl/program.hpp>
#include <lauf/impl/vm.hpp>
#include <lauf/vm_memory.hpp>

// Stores additionally data that don't get their own arguments in dispatch.
struct lauf_vm_process_impl : lauf::joined_allocation<lauf_vm_process_impl, lauf::vm_allocation>,
                              lauf::vm_memory<lauf_vm_process_impl>
{
    friend lauf::joined_allocation<lauf_vm_process_impl, lauf::vm_allocation>;

public:
    static lauf_vm_process create_null(lauf_vm vm)
    {
        return lauf_vm_process_impl::create(1024, vm, 1024);
    }

    static void start(lauf_vm_process& process, lauf_program program)
    {
        auto mod            = program.mod;
        process->_literals  = mod->literal_data();
        process->_functions = mod->function_begin();

        lauf::vm_memory<lauf_vm_process_impl>::allocate_program_memory(process,
                                                                       mod->allocation_data(),
                                                                       mod->allocation_data()
                                                                           + mod->allocation_count);
        process->_vm->process = process;
    }

    static void finish(lauf_vm_process process)
    {
        lauf::vm_memory<lauf_vm_process_impl>::free_process_memory(process,
                                                                   process->_vm->allocator);
    }

    // Override this one to update the back pointer properly.
    static lauf_value_address add_allocation(lauf_vm_process& process, lauf::vm_allocation alloc)
    {
        auto result = lauf::vm_memory<lauf_vm_process_impl>::add_allocation(process, alloc);
        process->_vm->process = process;
        return result;
    }
    static lauf_value_address add_local_allocations(lauf_vm_process&           process,
                                                    unsigned char*             local_memory,
                                                    const lauf::vm_allocation* alloc,
                                                    std::size_t                count)
    {
        auto result
            = lauf::vm_memory<lauf_vm_process_impl>::add_local_allocations(process, local_memory,
                                                                           alloc, count);
        process->_vm->process = process;
        return result;
    }

    lauf_vm vm() const
    {
        return _vm;
    }

    lauf_value get_literal(lauf::bc_literal_idx idx) const
    {
        return _literals[size_t(idx)];
    }
    lauf_function get_function(lauf::bc_function_idx idx) const
    {
        return _functions[size_t(idx)];
    }

private:
    explicit lauf_vm_process_impl(lauf_vm vm, std::size_t allocation_capacity)
    : lauf::vm_memory<lauf_vm_process_impl>(vm->memory_stack, allocation_capacity),
      _literals(nullptr), _functions(nullptr), _vm(vm)
    {}

    const lauf_value* _literals;
    lauf_function*    _functions;
    lauf_vm           _vm;
};

#endif // SRC_LAUF_IMPL_PROCESS_HPP_INCLUDED

