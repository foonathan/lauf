// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/type.h>

#include <lauf/support/align.hpp>
#include <lauf/support/stack_allocator.hpp>
#include <new>

lauf_layout lauf_array_layout(lauf_layout base, size_t length)
{
    auto base_size = lauf::round_to_multiple_of_alignment(base.size, base.alignment);
    return {base_size * length, base.alignment};
}

size_t lauf_array_element_offset(size_t index, lauf_layout base, size_t)
{
    auto base_size = lauf::round_to_multiple_of_alignment(base.size, base.alignment);
    return base_size * index;
}

lauf_layout lauf_aggregate_layout(const lauf_layout* members, size_t member_count)
{
    // Alignment is member with greatest alignment.
    auto alignment = [&] {
        auto result = size_t(0);
        for (auto i = 0u; i != member_count; ++i)
            if (members[i].alignment > result)
                result = members[i].alignment;
        return result;
    }();

    // Place members in order, adding padding where necessary.
    lauf::stack_allocator_offset allocator;
    for (auto i = 0u; i != member_count; ++i)
        allocator.allocate(members[i].size, members[i].alignment);
    auto size = allocator.size();

    return {size, alignment};
}

size_t lauf_aggregate_member_offset(size_t member_idx, const lauf_layout* members, size_t)
{
    // Same as above, but we're only computing the size until we've reached the member in question.
    lauf::stack_allocator_offset allocator;
    for (auto i = 0u; i != member_idx; ++i)
        allocator.allocate(members[i].size, members[i].alignment);
    return allocator.size();
}

namespace
{
lauf_value load_value(const void* object_address, size_t)
{
    return *static_cast<const lauf_value*>(object_address);
}

bool store_value(void* object_address, size_t, lauf_value value)
{
    ::new (object_address) lauf_value(value);
    return true;
}
} // namespace

const lauf_type_data lauf_value_type
    = {LAUF_NATIVE_LAYOUT_OF(lauf_value), 1, load_value, store_value};

