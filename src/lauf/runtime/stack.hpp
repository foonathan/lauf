// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_RUNTIME_STACK_HPP_INCLUDED
#define SRC_LAUF_RUNTIME_STACK_HPP_INCLUDED

#include <new>

#include <lauf/asm/module.hpp>
#include <lauf/config.h>
#include <lauf/support/page_allocator.hpp>

//=== stack frame ===//
struct lauf_runtime_stack_frame
{
    // The current function.
    const lauf_asm_function* function;
    // The return address to jump to when the call finishes.
    const lauf_asm_inst* return_ip;
    // The allocation of the first local_alloc (only meaningful if the function has any)
    uint32_t first_local_alloc : 30;
    // The generation of the local variables.
    uint32_t local_generation : 2;
    // The offset from this where the next frame can be put, i.e. after the local allocs.
    uint32_t next_offset;
    // The previous stack frame.
    lauf_runtime_stack_frame* prev;

    void assign_callstack_leaf_frame(const lauf_asm_inst* ip, lauf_runtime_stack_frame* frame_ptr)
    {
        return_ip = ip + 1;
        prev      = frame_ptr;
    }

    bool is_trampoline_frame() const
    {
        return prev == nullptr;
    }

    bool is_root_frame() const
    {
        return prev->is_trampoline_frame();
    }

    unsigned char* next_frame()
    {
        return reinterpret_cast<unsigned char*>(this) + next_offset;
    }
};
static_assert(alignof(lauf_runtime_stack_frame) == alignof(void*));

//=== cstack ===//
namespace lauf
{
class cstack
{
    // A chunk is always a single page.
    // That way we can easily determine the current check from the frame pointer.
    struct chunk
    {
        chunk* next;

        static chunk* allocate(page_allocator& alloc, std::size_t page_count)
        {
            assert(page_count >= 1);
            auto block = alloc.allocate(page_count);

            auto first = ::new (block.ptr) chunk{nullptr};
            auto cur   = first;
            for (auto i = 1u; i < page_count; ++i)
            {
                cur->next = ::new (cur->end()) chunk{nullptr};
                cur       = cur->next;
            }

            return first;
        }
        static chunk* deallocate(page_allocator& alloc, chunk* cur)
        {
            auto next = cur->next;
            alloc.deallocate({cur, 1});
            return next;
        }

        static chunk* chunk_of(void* address)
        {
            return static_cast<chunk*>(page_allocator::page_of(address));
        }

        unsigned char* memory()
        {
            return reinterpret_cast<unsigned char*>(this + 1);
        }
        unsigned char* end()
        {
            return reinterpret_cast<unsigned char*>(this) + page_allocator::page_size;
        }

        std::size_t remaining_space(unsigned char* next_frame)
        {
            return std::size_t(end() - next_frame);
        }
    };

public:
    void init(page_allocator& alloc, std::size_t initial_stack_size_in_bytes)
    {
        auto page_count = page_allocator::page_count_for(initial_stack_size_in_bytes);
        _first          = chunk::allocate(alloc, page_count);
    }

    void clear(page_allocator& alloc)
    {
        for (auto cur = _first; cur != nullptr; cur = chunk::deallocate(alloc, cur))
        {}

        _first = nullptr;
    }

    lauf_runtime_stack_frame* new_trampoline_frame(lauf_runtime_stack_frame* frame_ptr,
                                                   const lauf_asm_function*  callee)
    {
        auto next_frame = frame_ptr == nullptr ? _first->memory() : frame_ptr->next_frame();

        if (auto cur_chunk = chunk::chunk_of(next_frame); //
            LAUF_UNLIKELY(sizeof(lauf_runtime_stack_frame)
                          > cur_chunk->remaining_space(next_frame)))
        {
            if (LAUF_UNLIKELY(cur_chunk->next == nullptr))
                return nullptr;

            cur_chunk  = cur_chunk->next;
            next_frame = cur_chunk->memory();
        }

        return ::new (next_frame)
            lauf_runtime_stack_frame{callee, nullptr, 0, 0, sizeof(lauf_runtime_stack_frame),
                                     nullptr};
    }

    lauf_runtime_stack_frame* new_call_frame(lauf_runtime_stack_frame* frame_ptr,
                                             const lauf_asm_function*  callee,
                                             const lauf_asm_inst*      ip)
    {
        auto next_frame = frame_ptr->next_frame();

        if (auto cur_chunk = chunk::chunk_of(next_frame);
            LAUF_UNLIKELY(callee->max_cstack_size > cur_chunk->remaining_space(next_frame)))
        {
            if (LAUF_UNLIKELY(cur_chunk->next == nullptr))
                return nullptr;

            cur_chunk  = cur_chunk->next;
            next_frame = cur_chunk->memory();
        }

        return ::new (next_frame)
            lauf_runtime_stack_frame{callee,   ip + 1, 0, 0, sizeof(lauf_runtime_stack_frame),
                                     frame_ptr};
    }

    std::size_t allocate_more_stack_space(page_allocator& alloc, void* frame_ptr)
    {
        auto cur_chunk  = chunk::chunk_of(frame_ptr);
        cur_chunk->next = chunk::allocate(alloc, 1);
        return page_allocator::page_size;
    }

private:
    chunk* _first = nullptr;
};
} // namespace lauf

#endif // SRC_LAUF_RUNTIME_STACK_HPP_INCLUDED

