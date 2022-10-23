// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/support/page_allocator.hpp>

#include <new>
#include <sys/mman.h>
#include <unistd.h>

//#define LAUF_PAGE_ALLOCATOR_LOG
#ifdef LAUF_PAGE_ALLOCATOR_LOG

#    include <cstdio>
#    define LAUF_PAGE_ALLOCATOR_DO_LOG(...)                                                        \
        do                                                                                         \
        {                                                                                          \
            std::printf("[lauf] page_allocator: " __VA_ARGS__);                                    \
            std::putchar('\n');                                                                    \
        } while (0)

#else
#    define LAUF_PAGE_ALLOCATOR_DO_LOG(...)                                                        \
        do                                                                                         \
        {                                                                                          \
        } while (0)
#endif

struct lauf::page_allocator::free_list_node
{
    std::size_t     size;
    free_list_node* next;

    void* end()
    {
        return reinterpret_cast<unsigned char*>(this) + size;
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

lauf::page_block lauf::page_allocator::allocate(std::size_t size)
{
    size = round_to_multiple_of_alignment(size, real_page_size);

    // Find a page_count pages in the free list.
    free_list_node* prev = nullptr;
    for (auto cur = _free_list; cur != nullptr; prev = cur, cur = cur->next)
        if (cur->size >= size)
        {
            LAUF_PAGE_ALLOCATOR_DO_LOG("allocate(%zu): found %zu in cache", size, cur->size);

            // Remove block from free list.
            if (prev == nullptr)
                _free_list = cur->next;
            else
                prev->next = cur->next;

            // We're always returning the entire set of pages we know about.
            return {cur, cur->size};
        }

    // Allocate new set of pages.
    auto pages = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(pages != MAP_FAILED); // NOLINT: macro
    _allocated_bytes += size;

    LAUF_PAGE_ALLOCATOR_DO_LOG("allocate(%zu): mmap", size);
    return {pages, size};
}

std::size_t lauf::page_allocator::try_extend(page_block block, std::size_t new_size)
{
    // We know that we can't extend it using the free list:
    // * upon allocation we return the maximal sequence of contiguous blocks
    // * when deallocating we merge everything to keep it contiguous
    // So the only way to extend it is to ask the OS.

    new_size = round_to_multiple_of_alignment(new_size, real_page_size);

    auto ptr = ::mremap(block.ptr, block.size, new_size, 0);
    if (ptr == MAP_FAILED) // NOLINT: macro
    {
        LAUF_PAGE_ALLOCATOR_DO_LOG("try_extend({%p, %zu}, %zu): failed", block.ptr, block.size,
                                   new_size);
        return 0;
    }

    _allocated_bytes += new_size - block.size;
    assert(ptr == block.ptr);

    LAUF_PAGE_ALLOCATOR_DO_LOG("try_extend({%p, %zu}, %zu): failed", block.ptr, block.size,
                               new_size);
    return new_size;
}

void lauf::page_allocator::deallocate(page_block block)
{
    block.size = round_to_multiple_of_alignment(block.size, real_page_size);

    // Merge with an existing page block if possible.
    for (auto cur = _free_list; cur != nullptr; cur = cur->next)
        if (cur->end() == block.ptr)
        {
            cur->size += block.size;
            LAUF_PAGE_ALLOCATOR_DO_LOG("deallocate({%p, %zu}): merged", block.ptr, block.size);
            return;
        }

    // Add to free list.
    _free_list = ::new (block.ptr) free_list_node{block.size, _free_list};
    LAUF_PAGE_ALLOCATOR_DO_LOG("deallocate({%p, %zu}): not merged", block.ptr, block.size);
}

std::size_t lauf::page_allocator::release()
{
    auto cur = _free_list;
    while (cur != nullptr)
    {
        auto size = cur->size;
        auto next = cur->next;

        ::munmap(cur, size);
        _allocated_bytes -= size;

        cur = next;
    }

    return _allocated_bytes;
}

