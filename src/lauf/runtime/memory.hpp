// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_RUNTIME_MEMORY_HPP_INCLUDED
#define LAUF_RUNTIME_MEMORY_HPP_INCLUDED

#include <lauf/runtime/memory.h>

#include <lauf/asm/type.h>
#include <lauf/config.h>
#include <lauf/runtime/value.h>
#include <lauf/support/align.hpp>
#include <lauf/support/array.hpp>

typedef struct lauf_asm_module    lauf_asm_module;
typedef struct lauf_vm            lauf_vm;
typedef struct lauf_runtime_fiber lauf_runtime_fiber;

//=== allocation ===//
namespace lauf
{
enum class allocation_source : std::uint8_t
{
    static_const_memory,
    static_mut_memory,
    local_memory,
    heap_memory,
    // Memory stores metadata of a fiber.
    fiber_memory,
};

constexpr bool is_const(allocation_source source)
{
    switch (source)
    {
    case allocation_source::static_const_memory:
    case allocation_source::fiber_memory:
        return true;
    case allocation_source::static_mut_memory:
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

constexpr lauf::allocation make_fiber_alloc(lauf_runtime_fiber* f)
{
    lauf::allocation alloc;
    alloc.ptr        = f;
    alloc.size       = 0;
    alloc.source     = lauf::allocation_source::fiber_memory;
    alloc.status     = lauf::allocation_status::poison;
    alloc.generation = 0;
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

namespace lauf
{
/// The memory of a process.
class memory
{
public:
    memory() = default;

    memory(const memory&)            = delete;
    memory& operator=(const memory&) = delete;

    void init(lauf_vm* vm, const lauf_asm_module* mod);
    void clear(lauf_vm* vm);
    void destroy(lauf_vm* vm);

    //=== container interface ===//
    auto begin()
    {
        return _allocations.begin();
    }
    auto end()
    {
        return _allocations.end();
    }
    auto begin() const
    {
        return _allocations.begin();
    }
    auto end() const
    {
        return _allocations.end();
    }

    lauf_runtime_address new_allocation(page_allocator& allocator, allocation alloc)
    {
        auto index = _allocations.size();
        _allocations.push_back(allocator, alloc);
        return {std::uint32_t(index), alloc.generation, 0};
    }
    lauf_runtime_address new_allocation_unchecked(allocation alloc)
    {
        auto index = _allocations.size();
        _allocations.push_back_unchecked(alloc);
        return {std::uint32_t(index), alloc.generation, 0};
    }

    allocation& operator[](std::size_t index)
    {
        return _allocations[index];
    }
    const allocation& operator[](std::size_t index) const
    {
        return _allocations[index];
    }

    allocation* try_get(lauf_runtime_address addr)
    {
        if (LAUF_UNLIKELY(addr.allocation >= _allocations.size()))
            return nullptr;

        auto alloc = &_allocations[addr.allocation];
        if (LAUF_UNLIKELY((alloc->generation & 0b11) != addr.generation))
            return nullptr;

        return alloc;
    }

    //=== local allocations ===//
    bool needs_to_grow(std::size_t additional_allocations) const
    {
        return _allocations.size() + additional_allocations > _allocations.capacity();
    }
    void grow(page_allocator& allocator)
    {
        _allocations.reserve(allocator, _allocations.size() + 1);
    }

    std::uint32_t next_index() const
    {
        return std::uint32_t(_allocations.size());
    }
    std::uint8_t cur_generation() const
    {
        return _cur_generation;
    }

    // This function is called on frame entry of functions with local variables.
    // It will garbage collect allocations that have been freed.
    void remove_freed()
    {
        if (_allocations.empty() || _allocations.back().status != lauf::allocation_status::freed)
            // We only remove from the back.
            return;

        do
        {
            _allocations.pop_back();
        } while (!_allocations.empty()
                 && _allocations.back().status == lauf::allocation_status::freed);

        // Since we changed something, we need to increment the generation.
        ++_cur_generation;
    }

private:
    lauf::array<allocation> _allocations;
    std::uint8_t            _cur_generation = 0;
};
} // namespace lauf

#endif // LAUF_RUNTIME_MEMORY_HPP_INCLUDED

