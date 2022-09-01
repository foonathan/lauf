// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_SUPPORT_PAGE_ALLOCATOR_HPP_INCLUDED
#define SRC_LAUF_SUPPORT_PAGE_ALLOCATOR_HPP_INCLUDED

#include <lauf/config.h>
#include <lauf/support/align.hpp>

namespace lauf
{
struct page_block
{
    void*       ptr;
    std::size_t size;
};

class page_allocator
{
public:
    constexpr page_allocator() : _free_list(nullptr), _allocated_bytes(0) {}

    //=== page query ===//
    // We hardcode the page size to a compile-time constant that is <= and divisible by the actual
    // page size.
    static constexpr std::size_t page_size = 4096;

    static void* page_of(void* address)
    {
        auto bits = reinterpret_cast<std::uintptr_t>(address);
        assert(is_valid_alignment(page_size));
        auto misaligned = bits & std::uintptr_t(page_size - 1);
        return static_cast<unsigned char*>(address) - misaligned;
    }

    //=== allocation ===//
    page_block allocate(std::size_t size);

    bool try_extend(page_block& block, std::size_t size);

    /// Adds to a cache only.
    void deallocate(page_block block);

    /// Frees all pages from the cache.
    std::size_t release();

private:
    // Stored at the beginning of a free page block.
    struct free_list_node;

    free_list_node* _free_list;
    std::size_t     _allocated_bytes;
};
} // namespace lauf

#endif // SRC_LAUF_SUPPORT_PAGE_ALLOCATOR_HPP_INCLUDED

