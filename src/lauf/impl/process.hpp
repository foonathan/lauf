// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_IMPL_PROCESS_HPP_INCLUDED
#define SRC_LAUF_IMPL_PROCESS_HPP_INCLUDED

#include <cassert>
#include <lauf/bc/vm_memory.hpp>
#include <lauf/impl/module.hpp>
#include <lauf/impl/program.hpp>
#include <lauf/impl/vm.hpp>

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

    void start(lauf_program program)
    {
        auto mod   = program.mod;
        _literals  = mod->literal_data();
        _functions = mod->function_begin();

        allocate_program_memory(mod->allocation_data(),
                                mod->allocation_data() + mod->allocation_count);
        _vm->process = this;
    }

    void finish()
    {
        free_process_memory(_vm->allocator);
    }

    // Override this one to update the back pointer properly.
    LAUF_INLINE static void resize_allocation_list(lauf_vm_process& process)
    {
        lauf::vm_memory<lauf_vm_process_impl>::resize_allocation_list(process);
        process->_vm->process = process;
    }

    LAUF_INLINE lauf_vm vm() const
    {
        return _vm;
    }

    LAUF_INLINE lauf_value get_literal(lauf::bc_literal_idx idx) const
    {
        return _literals[size_t(idx)];
    }
    LAUF_INLINE lauf_function get_function(lauf::bc_function_idx idx) const
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

