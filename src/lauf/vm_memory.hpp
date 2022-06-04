// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_VM_MEMORY_HPP_INCLUDED
#define SRC_LAUF_VM_MEMORY_HPP_INCLUDED

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <lauf/config.h>
#include <lauf/support/joined_allocation.hpp>
#include <lauf/support/stack_allocator.hpp>
#include <lauf/type.h>
#include <lauf/vm.h>

namespace lauf
{
struct vm_allocation
{
    enum source_t : uint8_t
    {
        // Global const memory: no need to do anything on process start.
        static_const_memory,
        // Global mutable memory: allocate and copy on process start.
        static_mutable_memory,
        // Global zero memory: allocate and clear on process start.
        static_zero_memory,

        stack_memory,
        heap_memory,
    };
    enum lifetime_t : uint8_t
    {
        allocated,
        poisoned,
        freed,
    };
    enum split_t : uint8_t
    {
        unsplit,
        first_split,
        middle_split,
        last_split,
    };

    void*      ptr;
    uint32_t   size;
    source_t   source;
    lifetime_t lifetime = allocated;
    split_t    split    = unsplit;
    // We're storing an 8 bit generation here, even though the actual address remembers only two
    // bits. But we have the space, so why bother.
    uint8_t generation = 0;

    vm_allocation(uint32_t size, source_t source = static_zero_memory)
    : ptr(nullptr), size(size), source(source)
    {}
    vm_allocation(const void* ptr, uint32_t size, source_t source = static_const_memory)
    : ptr(const_cast<void*>(ptr)), size(size), source(source)
    {}
    vm_allocation(void* ptr, uint32_t size) = delete;
    vm_allocation(void* ptr, uint32_t size, source_t source)
    : ptr(const_cast<void*>(ptr)), size(size), source(source)
    {}

    // UNCHECKED
    LAUF_INLINE void* offset(std::size_t o) const
    {
        return static_cast<unsigned char*>(ptr) + o;
    }

    LAUF_INLINE bool is_split() const
    {
        return split != vm_allocation::unsplit;
    }
};
static_assert(sizeof(vm_allocation) == 2 * sizeof(void*));
} // namespace lauf

namespace lauf
{
template <typename Derived>
class vm_memory
{
public:
    explicit vm_memory(memory_stack& stack, std::size_t capacity)
    : _capacity(capacity), _first_unused(0), _generation(0), _allocator(stack)
    {}

    //=== allocation setup ===//
    LAUF_INLINE bool has_capacity_for_allocations(std::size_t count) const
    {
        return _first_unused + count < _capacity;
    }

    static void resize_allocation_list(Derived*& ptr)
    {
        auto self = static_cast<vm_memory*>(ptr);

        auto new_capacity = 2ull * self->_capacity;
        Derived::resize(ptr, {self->_capacity}, {new_capacity});

        self            = ptr;
        self->_capacity = new_capacity;
    }

    LAUF_INLINE lauf_value_address add_allocation(vm_allocation alloc)
    {
        alloc.generation                 = _generation;
        get_allocations()[_first_unused] = alloc;
        return {_first_unused++, _generation, 0};
    }

    LAUF_INLINE lauf_value_address add_local_allocations(unsigned char*       local_memory,
                                                         const vm_allocation* allocs,
                                                         std::size_t          count)
    {
        if (count == 0)
            return lauf_value_address_invalid;

        auto first_alloc = _first_unused;
        auto dest        = get_allocations() + first_alloc;
        for (auto i = 0u; i != count; ++i)
        {
            *dest            = allocs[i];
            dest->generation = _generation;
            dest->ptr        = local_memory + reinterpret_cast<std::uintptr_t>(dest->ptr);
            ++dest;
        }
        _first_unused += count;

        return {first_alloc, _generation, 0};
    }

    LAUF_INLINE vm_allocation* remove_allocation(lauf_value_address addr)
    {
        auto alloc = get_allocation(addr);
        if (alloc == nullptr)
            return nullptr;
        alloc->lifetime = vm_allocation::freed;

        if (addr.allocation == _first_unused - 1u)
        {
            // We can remove the allocation as its at the end.
            // Potentially, we can also remove more subsequent allocations.
            do
            {
                --_first_unused;
                // We increment the generation to detect use-after free.
                ++_generation;
            } while (_first_unused > 0u
                     && get_allocations()[_first_unused - 1u].lifetime == vm_allocation::freed);
        }

        return alloc;
    }

    void allocate_program_memory(vm_allocation* begin, vm_allocation* end)
    {
        assert(has_capacity_for_allocations(std::ptrdiff_t(end - begin)));

        for (auto ptr = begin; ptr != end; ++ptr)
        {
            auto alloc = *ptr;
            if (alloc.source == vm_allocation::static_zero_memory)
            {
                auto ptr = _allocator.template allocate<alignof(void*)>(alloc.size);
                std::memset(ptr, 0, alloc.size);
                alloc.ptr = ptr;
            }
            else if (alloc.source == vm_allocation::static_mutable_memory)
            {
                auto ptr = _allocator.template allocate<alignof(void*)>(alloc.size);
                std::memcpy(ptr, alloc.ptr, alloc.size);
                alloc.ptr = ptr;
            }

            add_allocation(alloc);
        }
    }

    void free_process_memory(lauf_vm_allocator heap)
    {
        for (auto idx = uint32_t(0); idx != _first_unused; ++idx)
        {
            auto alloc = get_allocations()[idx];
            if (alloc.source == vm_allocation::heap_memory
                && alloc.lifetime != vm_allocation::freed
                // Either the allocation isn't split, or it's the first split.
                && (alloc.split == vm_allocation::unsplit
                    || alloc.split == vm_allocation::first_split))
                heap.free_alloc(heap.user_data, alloc.ptr);
        }

        _first_unused = 0;
    }

    //=== allocators ===//
    LAUF_INLINE stack_allocator& stack()
    {
        return _allocator;
    }

    //=== memory access ===//
    LAUF_INLINE vm_allocation* get_allocation(lauf_value_address addr)
    {
        if (addr.allocation >= _capacity)
            return nullptr;

        auto alloc = get_allocations() + std::ptrdiff_t(addr.allocation);
        if (alloc->lifetime == lauf::vm_allocation::freed
            || (alloc->generation & 0b11) != std::uint8_t(addr.generation))
            return nullptr;
        return alloc;
    }

    LAUF_INLINE const void* get_const_ptr(lauf_value_address addr, lauf_layout layout)
    {
        if (auto alloc = get_allocation(addr))
        {
            if (std::ptrdiff_t(alloc->size) - std::ptrdiff_t(addr.offset) < layout.size
                || alloc->lifetime != vm_allocation::allocated)
                return nullptr;

            auto ptr = alloc->offset(addr.offset);
            if (is_aligned(ptr, layout.alignment))
                return ptr;
        }

        return nullptr;
    }
    LAUF_INLINE void* get_mutable_ptr(lauf_value_address addr, lauf_layout layout)
    {
        if (auto alloc = get_allocation(addr))
        {
            if (ptrdiff_t(alloc->size) - std::ptrdiff_t(addr.offset) < layout.size
                || alloc->lifetime != vm_allocation::allocated
                || alloc->source == vm_allocation::static_const_memory)
                return nullptr;

            auto ptr = alloc->offset(addr.offset);
            if (is_aligned(ptr, layout.alignment))
                return ptr;
        }

        return nullptr;
    }

    LAUF_INLINE const char* get_const_cstr(lauf_value_address addr)
    {
        if (auto alloc = get_allocation(addr))
        {
            if (addr.offset >= alloc->size || alloc->lifetime != vm_allocation::allocated)
                return nullptr;

            auto cstr = static_cast<const char*>(alloc->offset(addr.offset));
            if (std::memchr(cstr, 0, alloc->size - addr.offset) == nullptr)
                return nullptr;
            return cstr;
        }

        return nullptr;
    }
    LAUF_INLINE const char* get_mutable_cstr(lauf_value_address addr)
    {
        if (auto alloc = get_allocation(addr))
        {
            if (addr.offset >= alloc->size || alloc->lifetime != vm_allocation::allocated
                || alloc->source == vm_allocation::static_const_memory)
                return nullptr;

            auto cstr = static_cast<const char*>(alloc->offset(addr.offset));
            if (std::memchr(cstr, 0, alloc->size - addr.offset) == nullptr)
                return nullptr;
            return cstr;
        }

        return nullptr;
    }

protected:
    ~vm_memory() = default;

private:
    LAUF_INLINE vm_allocation* get_allocations()
    {
        return static_cast<Derived&>(*this).template array<vm_allocation>({});
    }

    std::uint32_t   _capacity;
    std::uint32_t   _first_unused : 30;
    std::uint32_t   _generation : 2;
    stack_allocator _allocator;
};
} // namespace lauf

#endif // SRC_LAUF_VM_MEMORY_HPP_INCLUDED

