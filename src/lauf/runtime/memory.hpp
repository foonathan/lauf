// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_RUNTIME_MEMORY_HPP_INCLUDED
#define LAUF_RUNTIME_MEMORY_HPP_INCLUDED

#include <lauf/asm/type.h>
#include <lauf/config.h>
#include <lauf/runtime/value.h>
#include <lauf/support/align.hpp>

//=== allocation ===//
namespace lauf
{
enum class allocation_source : std::uint8_t
{
    static_const_memory,
    static_mut_memory,
    local_memory,
    heap_memory,
};

constexpr bool is_const(allocation_source source)
{
    switch (source)
    {
    case allocation_source::static_const_memory:
        return true;
    case allocation_source::static_mut_memory:
    case allocation_source::local_memory:
    case allocation_source::heap_memory:
        return false;
    }
}

constexpr bool is_static(allocation_source source)
{
    switch (source)
    {
    case allocation_source::static_const_memory:
    case allocation_source::static_mut_memory:
        return true;
    case allocation_source::local_memory:
    case allocation_source::heap_memory:
        return false;
    }
}

enum class allocation_status : std::uint8_t
{
    allocated,
    freed,
    poison,
};

constexpr bool is_usable(allocation_status status)
{
    return status == allocation_status::allocated;
}
constexpr bool can_be_freed(allocation_status status)
{
    return status != allocation_status::freed;
}

enum class allocation_split : std::uint8_t
{
    unsplit,
    split_first,  // ptr is the actual beginning of the original allocation.
    split_middle, // ptr is somewhere in the middle of the allocation.
    split_last,   // ptr + size is the actual end of the original allocation.
};

enum class gc_tracking : std::uint8_t
{
    // allocation is not reachable/reachability hasn't been determined yet.
    unreachable,
    // allocation is reachable.
    reachable,
    // allocation has been explicitly marked as reachable.
    reachable_explicit,
};

struct allocation
{
    void*             ptr;
    std::uint32_t     size;
    std::uint8_t      generation;
    allocation_source source;
    allocation_status status;
    allocation_split  split : 2      = allocation_split::unsplit;
    gc_tracking       gc : 2         = gc_tracking::unreachable;
    bool              is_gc_weak : 1 = false;

    constexpr void* unchecked_offset(std::uint32_t offset) const
    {
        return static_cast<unsigned char*>(ptr) + offset;
    }
};
static_assert(sizeof(allocation) == 2 * sizeof(void*));

constexpr lauf::allocation make_local_alloc(void* memory, std::size_t size, std::uint8_t generation)
{
    lauf::allocation alloc;
    alloc.ptr        = memory;
    alloc.size       = std::uint32_t(size);
    alloc.source     = lauf::allocation_source::local_memory;
    alloc.status     = lauf::allocation_status::allocated;
    alloc.gc         = lauf::gc_tracking::reachable_explicit;
    alloc.generation = generation;
    return alloc;
}

constexpr lauf::allocation make_heap_alloc(void* memory, std::size_t size, std::uint8_t generation)
{
    lauf::allocation alloc;
    alloc.ptr        = memory;
    alloc.size       = std::uint32_t(size);
    alloc.source     = lauf::allocation_source::heap_memory;
    alloc.status     = lauf::allocation_status::allocated;
    alloc.generation = generation;
    return alloc;
}
} // namespace lauf

namespace lauf
{
inline const void* checked_offset(lauf::allocation alloc, lauf_runtime_address addr,
                                  lauf_asm_layout layout)
{
    if (LAUF_UNLIKELY(!lauf::is_usable(alloc.status)))
        return nullptr;

    if (LAUF_UNLIKELY(addr.offset + layout.size > alloc.size))
        return nullptr;

    auto ptr = alloc.unchecked_offset(addr.offset);
    if (LAUF_UNLIKELY(!lauf::is_aligned(ptr, layout.alignment)))
        return nullptr;

    return ptr;
}

inline const void* checked_offset(lauf::allocation alloc, lauf_runtime_address addr)
{
    if (LAUF_UNLIKELY(!lauf::is_usable(alloc.status)))
        return nullptr;

    if (LAUF_UNLIKELY(addr.offset >= alloc.size))
        return nullptr;

    return alloc.unchecked_offset(addr.offset);
}
} // namespace lauf

#endif // LAUF_RUNTIME_MEMORY_HPP_INCLUDED

