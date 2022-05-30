// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_IMPL_MODULE_HPP_INCLUDED
#define SRC_IMPL_MODULE_HPP_INCLUDED

#include <cstring>
#include <lauf/detail/bytecode.hpp>
#include <lauf/module.h>
#include <lauf/value.h>
#include <memory>

//=== debug metadata ===//
namespace lauf::_detail
{
class debug_location_map
{
public:
    struct entry
    {
        ptrdiff_t           first_address;
        lauf_debug_location location;
    };

    debug_location_map() = default;
    explicit debug_location_map(const entry* entries, std::size_t size)
    : _entries(size == 0 ? nullptr : new entry[1 + size])
    {
        if (size > 0)
        {
            std::memcpy(_entries.get(), entries, size * sizeof(entry));
            _entries.get()[size] = entry{PTRDIFF_MAX, {}};
        }
    }
    lauf_debug_location location_of(ptrdiff_t offset)
    {
        if (_entries == nullptr)
            return {};

        // This can be optimized to a binary search at the cost of storing the extra size.
        auto cur = _entries.get();
        auto loc = cur->location;
        while (cur->first_address <= offset)
        {
            loc = cur->location;
            ++cur;
        }

        return loc;
    }

private:
    // last entry is sentinel
    std::unique_ptr<entry[]> _entries;
};
} // namespace lauf::_detail

//=== allocation ===//
namespace lauf::_detail
{
struct allocation
{
    enum flag : uint32_t
    {
        is_const       = 1 << 0,
        is_poisoned    = 1 << 1,
        is_split       = 1 << 2,
        is_first_split = 1 << 3,
        is_last_split  = 1 << 4,

        // Memory that needs to be allocated when the program is created.
        static_memory = 1 << 5,
        // Memory needs to be copied over before execution.
        copy_memory = 1 << 6,
        // Memory needs to be cleared before execution.
        clear_memory = 1 << 7,

        // Memory that is heap allocated.
        heap_memory = 1 << 8,
        // Memory that is already freed.
        freed_memory = 1 << 9,
    };

    void*    ptr;
    uint32_t size;
    uint16_t flags;
    // We're storing a 16 bit generation here, even though the actual address remembers only two
    // bits. But we have the space, so why bother.
    uint16_t generation;

    allocation(uint32_t size, uint16_t flags = static_memory | clear_memory)
    : ptr(nullptr), size(size), flags(flags), generation(0)
    {}
    allocation(const void* ptr, uint32_t size, uint16_t flags = is_const)
    : ptr(const_cast<void*>(ptr)), size(size), flags(flags), generation(0)
    {}
    allocation(void* ptr, uint32_t size, uint16_t flags = 0)
    : ptr(const_cast<void*>(ptr)), size(size), flags(flags), generation(0)
    {}

    void* offset(std::size_t o) const
    {
        return static_cast<unsigned char*>(ptr) + o;
    }
};
static_assert(sizeof(allocation) == 2 * sizeof(void*));
} // namespace lauf::_detail

//=== function ===//
struct lauf_function_impl
{
    lauf_module                       mod;
    const char*                       name;
    uint16_t                          _padding;
    uint16_t                          local_stack_size;
    uint16_t                          max_vstack_size;
    uint8_t                           input_count;
    uint8_t                           output_count;
    lauf::_detail::debug_location_map debug_locations;

    lauf_vm_instruction* bytecode()
    {
        auto memory = static_cast<void*>(this + 1);
        return static_cast<lauf_vm_instruction*>(memory);
    }
};
static_assert(sizeof(lauf_function_impl) == 3 * sizeof(void*) + sizeof(uint64_t));
static_assert(sizeof(lauf_function_impl) % alignof(uint32_t) == 0);

lauf_function lauf_impl_allocate_function(size_t bytecode_size);

//=== module ===//
struct alignas(lauf_value) lauf_module_impl
{
    const char* name;
    const char* path;
    size_t      function_count, literal_count, allocation_count;

    lauf_function* function_begin()
    {
        auto memory = static_cast<void*>(this + 1);
        return static_cast<lauf_function*>(memory);
    }
    lauf_function* function_end()
    {
        return function_begin() + function_count;
    }

    lauf_value* literal_data()
    {
        auto memory = static_cast<void*>(function_end());
        return static_cast<lauf_value*>(memory);
    }

    lauf::_detail::allocation* allocation_data()
    {
        auto memory = static_cast<void*>(literal_data() + literal_count);
        return static_cast<lauf::_detail::allocation*>(memory);
    }
};
static_assert(sizeof(lauf_module_impl) == 5 * sizeof(void*));
static_assert(sizeof(lauf_module_impl) % alignof(lauf_value) == 0);

lauf_module lauf_impl_allocate_module(size_t function_count, size_t literal_count,
                                      size_t allocation_count);

#endif // SRC_IMPL_MODULE_HPP_INCLUDED

