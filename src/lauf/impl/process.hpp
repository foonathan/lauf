// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_IMPL_PROCESS_HPP_INCLUDED
#define SRC_LAUF_IMPL_PROCESS_HPP_INCLUDED

#include <cassert>
#include <cstring>
#include <lauf/detail/stack_allocator.hpp>
#include <lauf/impl/module.hpp>
#include <lauf/impl/program.hpp>
#include <lauf/impl/vm.hpp>
#include <new>

namespace lauf::_detail
{
constexpr std::size_t initial_allocation_list_size = 1024;

}

// Stores additionally data that don't get their own arguments in dispatch.
struct alignas(lauf::_detail::allocation) lauf_vm_process_impl
{
    const lauf_value*              literals;
    lauf_function*                 functions;
    lauf_vm                        vm;
    uint32_t                       allocation_list_capacity;
    uint32_t                       first_unused_allocation : 30;
    uint32_t                       generation : 2;
    lauf::_detail::stack_allocator allocator;

    explicit lauf_vm_process_impl(lauf_vm vm)
    : literals(nullptr), functions(nullptr), vm(vm),
      allocation_list_capacity(lauf::_detail::initial_allocation_list_size),
      first_unused_allocation(0), generation(0), allocator(vm->memory_stack)
    {}

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
    auto get_allocation(lauf_value_address addr) -> lauf::_detail::allocation*
    {
        if (addr.allocation >= allocation_list_capacity)
            return nullptr;

        auto alloc = allocation_data() + ptrdiff_t(addr.allocation);
        if ((alloc->flags & lauf::_detail::allocation::freed_memory) != 0
            || (alloc->generation & 0b11) != uint16_t(addr.generation))
            return nullptr;
        return alloc;
    }

    const void* get_const_ptr(lauf_value_address addr, size_t size)
    {
        using lauf::_detail::allocation;
        if (auto alloc = get_allocation(addr))
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
        if (auto alloc = get_allocation(addr))
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
        if (auto alloc = get_allocation(addr))
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
        if (auto alloc = get_allocation(addr))
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
inline lauf_vm_process create_null_process(lauf_vm vm)
{
    auto memory = ::operator new(sizeof(lauf_vm_process_impl)
                                 + initial_allocation_list_size * sizeof(allocation));

    return ::new (memory) lauf_vm_process_impl(vm);
}

inline void destroy_process(lauf_vm_process process)
{
    ::operator delete(process);
}

// Pass pointer by references as we might need to do a realloc.
inline lauf_value_address add_allocation(lauf_vm_process& process, allocation alloc)
{
    if (alloc.size == 0)
        return lauf_value_address_invalid;

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

    alloc.generation                                             = process->generation;
    process->allocation_data()[process->first_unused_allocation] = alloc;
    return {process->first_unused_allocation++, process->generation, 0};
}

inline allocation* remove_allocation(lauf_vm_process& process, lauf_value_address addr)
{
    auto alloc = process->get_allocation(addr);
    if (alloc == nullptr)
        return nullptr;
    alloc->flags |= allocation::freed_memory;

    if (addr.allocation == process->first_unused_allocation - 1u)
    {
        // We can remove the allocation as its at the end.
        // Potentially, we can also remove more subsequent allocations.
        do
        {
            --process->first_unused_allocation;
        } while (process->first_unused_allocation > 0u
                 && (process->allocation_data()[process->first_unused_allocation - 1u].flags
                     & allocation::freed_memory)
                        != 0);

        // We increment the generation to detect use-after free.
        ++process->generation;
    }

    return alloc;
}

inline void init_process(lauf_vm_process process, lauf_program program)
{
    process->literals                = program.mod->literal_data();
    process->functions               = program.mod->function_begin();
    process->first_unused_allocation = 0;

    for (auto ptr = program.mod->allocation_data();
         ptr != program.mod->allocation_data() + program.mod->allocation_count; ++ptr)
    {
        auto alloc = *ptr;
        if ((alloc.flags & allocation::static_memory) != 0)
        {
            auto ptr = process->allocator.allocate<alignof(void*)>(alloc.size);
            if ((alloc.flags & allocation::clear_memory) != 0)
                std::memset(ptr, 0, alloc.size);
            else if ((alloc.flags & allocation::copy_memory) != 0)
                std::memcpy(ptr, alloc.ptr, alloc.size);
            alloc.ptr = ptr;
        }

        add_allocation(process, alloc);
    }
}

inline void reset_process(lauf_vm_process process)
{
    // Free all heap memory that is still allocated.
    for (auto idx = uint32_t(0); idx != process->first_unused_allocation; ++idx)
    {
        auto alloc = process->allocation_data()[idx];
        if ((alloc.flags & allocation::heap_memory) != 0
            && (alloc.flags & allocation::freed_memory) == 0)
            process->vm->allocator.free_alloc(process->vm->allocator.user_data,
                                              {alloc.ptr, alloc.size});
    }
}
} // namespace lauf::_detail

#endif // SRC_LAUF_IMPL_PROCESS_HPP_INCLUDED
