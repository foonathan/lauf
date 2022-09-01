// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/support/page_allocator.hpp>

#include <new>
#include <sys/mman.h>
#include <unistd.h>

struct lauf::page_allocator::free_list_node
{
    std::size_t     page_count;
    free_list_node* next;

    void* get_page(std::size_t idx)
    {
        return reinterpret_cast<unsigned char*>(this) + page_size * idx;
    }

    void* end()
    {
        return get_page(page_count);
    }
};

namespace
{
const auto real_page_size = [] {
    auto result = static_cast<std::size_t>(::sysconf(_SC_PAGE_SIZE));
    assert(lauf::page_allocator::page_size <= result);
    assert(result % lauf::page_allocator::page_size == 0);
    return result;
}();
}

lauf::page_block lauf::page_allocator::allocate(std::size_t page_count)
{
    // Find a page_count pages in the free list.
    free_list_node* prev = nullptr;
    for (auto cur = _free_list; cur != nullptr; prev = cur, cur = cur->next)
        if (cur->page_count >= page_count)
        {
            // Remove block from free list.
            if (prev == nullptr)
                _free_list = cur->next;
            else
                prev->next = cur->next;

            // We're always returning the entire set of pages we know about.
            return {cur, cur->page_count};
        }

    // Allocate new set of pages.
    auto size  = page_count * real_page_size;
    auto pages = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(pages != MAP_FAILED); // NOLINT: macro
    _allocated_bytes += size;

    return {pages, size / page_size};
}

bool lauf::page_allocator::try_extend(page_block& block, std::size_t new_page_count)
{
    // We know that we can't extend it using the free list:
    // * upon allocation we return the maximal sequence of contiguous blocks
    // * when deallocating we merge everything to keep it contiguous
    // So the only way to extend it is to ask the OS.

    auto new_size = new_page_count * real_page_size;
    auto ptr      = ::mremap(block.ptr, block.page_count * page_size, new_size, 0);
    if (ptr == MAP_FAILED) // NOLINT: macro
        return false;
    _allocated_bytes -= block.page_count * page_size;
    _allocated_bytes += new_size;

    assert(ptr == block.ptr);
    block.page_count = new_size / page_size;
    return true;
}

void lauf::page_allocator::deallocate(page_block block)
{
    // Merge with an existing page block if possible.
    for (auto cur = _free_list; cur != nullptr; cur = cur->next)
        if (cur->end() == block.ptr)
        {
            cur->page_count += block.page_count;
            return;
        }

    // Add to free list.
    _free_list = ::new (block.ptr) free_list_node{block.page_count, _free_list};
}

std::size_t lauf::page_allocator::release()
{
    auto cur = _free_list;
    while (cur != nullptr)
    {
        auto size = cur->page_count * page_size;
        auto next = cur->next;

        ::munmap(cur, size);
        _allocated_bytes -= size;

        cur = next;
    }

    return _allocated_bytes;
}

