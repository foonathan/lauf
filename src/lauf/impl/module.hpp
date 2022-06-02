// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_IMPL_MODULE_HPP_INCLUDED
#define SRC_IMPL_MODULE_HPP_INCLUDED

#include <cstring>
#include <lauf/bytecode.hpp>
#include <lauf/module.h>
#include <lauf/support/joined_allocation.hpp>
#include <lauf/value.h>
#include <lauf/vm_memory.hpp>
#include <memory>

//=== debug metadata ===//
namespace lauf
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
} // namespace lauf

//=== function ===//
struct lauf_function_impl
: lauf::joined_allocation<lauf_function_impl, lauf::vm_allocation, lauf_vm_instruction>
{
    lauf_module              mod;
    const char*              name;
    uint16_t                 max_vstack_size;
    uint16_t                 local_stack_size;
    uint16_t                 local_allocation_count;
    uint8_t                  input_count;
    uint8_t                  output_count;
    lauf::debug_location_map debug_locations;

    lauf_function_impl() = default;

    // The ptr is actually the offset from the start of the local memory.
    lauf::vm_allocation* local_allocations()
    {
        return array<lauf::vm_allocation>({});
    }

    lauf_vm_instruction* bytecode()
    {
        return array<lauf_vm_instruction>({local_allocation_count});
    }
};
static_assert(sizeof(lauf_function_impl) == 3 * sizeof(void*) + sizeof(uint64_t));

//=== module ===//
struct alignas(lauf_value) lauf_module_impl
: lauf::joined_allocation<lauf_module_impl, lauf_function, lauf_value, lauf::vm_allocation>
{
    const char* name;
    const char* path;
    size_t      function_count, literal_count, allocation_count;

    lauf_module_impl() = default;
    ~lauf_module_impl()
    {
        for (auto fn = function_begin(); fn != function_end(); ++fn)
            lauf_function_impl::destroy(*fn);
    }

    lauf_function* function_begin()
    {
        return array<lauf_function>({function_count, literal_count, allocation_count});
    }
    lauf_function* function_end()
    {
        return function_begin() + function_count;
    }

    lauf_value* literal_data()
    {
        return array<lauf_value>({function_count, literal_count, allocation_count});
    }

    lauf::vm_allocation* allocation_data()
    {
        return array<lauf::vm_allocation>({function_count, literal_count, allocation_count});
    }
};
static_assert(sizeof(lauf_module_impl) == 5 * sizeof(void*));

#endif // SRC_IMPL_MODULE_HPP_INCLUDED

