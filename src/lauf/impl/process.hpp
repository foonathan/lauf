// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_IMPL_PROCESS_HPP_INCLUDED
#define SRC_LAUF_IMPL_PROCESS_HPP_INCLUDED

#include <cstring>
#include <lauf/detail/stack_allocator.hpp>
#include <lauf/impl/module.hpp>
#include <lauf/impl/program.hpp>
#include <lauf/impl/vm.hpp>
#include <new>

// Stores additionally data that don't get their own arguments in dispatch.
struct alignas(lauf::_detail::allocation) lauf_vm_process_impl
{
    const lauf_value*              literals;
    lauf_function*                 functions;
    lauf_vm                        vm;
    uint32_t                       allocation_list_capacity;
    uint32_t                       first_unused_allocation;
    lauf::_detail::stack_allocator allocator;

    lauf_value get_literal(lauf::_detail::bc_literal_idx idx) const
    {
        return literals[size_t(idx)];
    }
    lauf_function get_function(lauf::_detail::bc_function_idx idx) const
    {
        return functions[size_t(idx)];
    }

    auto allocation_data()
    {
        auto memory = static_cast<void*>(this + 1);
        return static_cast<lauf::_detail::allocation*>(memory);
    }
    auto get_allocation(uint32_t allocation)
    {
        return allocation < allocation_list_capacity ? allocation_data() + allocation : nullptr;
    }

    auto get_local_address(uint32_t offset)
    {
        return lauf_value_address{first_unused_allocation - 1, offset};
    }

    const void* get_const_ptr(lauf_value_address addr, size_t size)
    {
        using lauf::_detail::allocation;
        if (auto alloc = get_allocation(addr.allocation))
        {
            if (ptrdiff_t(alloc->size) - ptrdiff_t(addr.offset) < size
                || (alloc->flags & allocation::is_poisoned) != 0)
                return nullptr;

            return alloc->offset(addr.offset);
        }

        return nullptr;
    }
    void* get_mutable_ptr(lauf_value_address addr, size_t size)
    {
        using lauf::_detail::allocation;
        if (auto alloc = get_allocation(addr.allocation))
        {
            if (ptrdiff_t(alloc->size) - ptrdiff_t(addr.offset) < size
                || (alloc->flags & allocation::is_poisoned) != 0
                || (alloc->flags & allocation::is_const) != 0)
                return nullptr;

            return alloc->offset(addr.offset);
        }

        return nullptr;
    }

    const char* get_const_cstr(lauf_value_address addr)
    {
        using lauf::_detail::allocation;
        if (auto alloc = get_allocation(addr.allocation))
        {
            if (addr.offset >= alloc->size)
                return nullptr;

            auto cstr = static_cast<const char*>(alloc->offset(addr.offset));
            if (std::memchr(cstr, 0, alloc->size - addr.offset) == nullptr)
                return nullptr;
            return cstr;
        }

        return nullptr;
    }
    const char* get_mutable_cstr(lauf_value_address addr)
    {
        using lauf::_detail::allocation;
        if (auto alloc = get_allocation(addr.allocation))
        {
            if (addr.offset >= alloc->size)
                return nullptr;

            auto cstr = static_cast<const char*>(alloc->offset(addr.offset));
            if (std::memchr(cstr, 0, alloc->size - addr.offset) == nullptr)
                return nullptr;
            return cstr;
        }

        return nullptr;
    }
};

namespace lauf::_detail
{
constexpr std::size_t initial_allocation_list_size = 8192;

inline lauf_vm_process create_null_process(lauf_vm vm)
{
    auto memory = ::operator new(sizeof(lauf_vm_process_impl)
                                 + initial_allocation_list_size * sizeof(allocation));

    auto result = ::new (memory) lauf_vm_process_impl{nullptr, nullptr,
                                                      vm,      initial_allocation_list_size,
                                                      0,       stack_allocator(vm->memory_stack)};
    return result;
}

inline void destroy_process(lauf_vm_process process)
{
    ::operator delete(process);
}

// Pass pointer by references as we might need to do a realloc.
inline void add_allocation(lauf_vm_process& process, allocation alloc)
{
    if (process->first_unused_allocation == process->allocation_list_capacity)
    {
        auto new_capacity = 2ull * process->allocation_list_capacity;

        auto new_size   = sizeof(lauf_vm_process_impl) + new_capacity * sizeof(allocation);
        auto new_memory = ::operator new(new_size);

        auto new_process = ::new (new_memory) lauf_vm_process_impl(*process);
        std::memcpy(new_process->allocation_data(), process->allocation_data(),
                    process->allocation_list_capacity * sizeof(allocation));
        new_process->allocation_list_capacity = new_capacity;

        process              = new_process;
        process->vm->process = process;
    }

    process->allocation_data()[process->first_unused_allocation] = alloc;
    ++process->first_unused_allocation;
}

inline void init_process(lauf_vm_process process, lauf_program program)
{
    process->literals                = program->mod->literal_data();
    process->functions               = program->mod->function_begin();
    process->first_unused_allocation = 0;

    auto                   static_memory = program->static_memory();
    stack_allocator_offset allocator;
    for (auto ptr = program->mod->allocation_data();
         ptr != program->mod->allocation_data() + program->mod->allocation_count; ++ptr)
    {
        auto alloc = *ptr;
        if ((alloc.flags & allocation::static_memory) != 0)
        {
            auto offset = allocator.allocate(alloc.size);
            if ((alloc.flags & allocation::clear_memory) != 0)
                std::memset(static_memory + offset, 0, alloc.size);
            else if ((alloc.flags & allocation::copy_memory) != 0)
                std::memcpy(static_memory + offset, alloc.ptr, alloc.size);
            alloc.ptr = static_memory + offset;
        }

        add_allocation(process, alloc);
    }
}
} // namespace lauf::_detail

#endif // SRC_LAUF_IMPL_PROCESS_HPP_INCLUDED

