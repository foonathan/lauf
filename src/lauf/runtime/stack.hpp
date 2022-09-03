// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_RUNTIME_STACK_HPP_INCLUDED
#define SRC_LAUF_RUNTIME_STACK_HPP_INCLUDED

#include <new>

#include <lauf/asm/module.hpp>
#include <lauf/config.h>
#include <lauf/runtime/value.h>
#include <lauf/support/page_allocator.hpp>

//=== stack frame ===//
struct lauf_runtime_stack_frame
{
    // The current function.
    const lauf_asm_function* function = nullptr;
    // The return address to jump to when the call finishes.
    const lauf_asm_inst* return_ip = nullptr;
    // The allocation of the first local_alloc (only meaningful if the function has any)
    uint32_t first_local_alloc : 30 = 0;
    // The generation of the local variables.
    uint32_t local_generation : 2 = 0;
    // The offset from this where the next frame can be put, i.e. after the local allocs.
    uint32_t next_offset = 0;
    // The previous stack frame.
    lauf_runtime_stack_frame* prev = nullptr;

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

        static chunk* allocate(page_allocator& alloc, std::size_t capacity)
        {
            assert(capacity > 0);
            auto block = alloc.allocate(capacity);

            auto first = ::new (block.ptr) chunk{nullptr};
            auto cur   = first;
            for (auto i = page_allocator::page_size; i < block.size; i += page_allocator::page_size)
            {
                cur->next = ::new (cur->end()) chunk{nullptr};
                cur       = cur->next;
            }

            return first;
        }
        static chunk* deallocate(page_allocator& alloc, chunk* cur)
        {
            auto next = cur->next;
            alloc.deallocate({cur, page_allocator::page_size});
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
        _first    = chunk::allocate(alloc, initial_stack_size_in_bytes);
        _capacity = initial_stack_size_in_bytes;
    }

    void clear(page_allocator& alloc)
    {
        for (auto cur = _first; cur != nullptr; cur = chunk::deallocate(alloc, cur))
        {}

        _first = nullptr;
    }

    void* base()
    {
        return _first->memory();
    }

    lauf_runtime_stack_frame* new_call_frame(lauf_runtime_stack_frame* frame_ptr,
                                             const lauf_asm_function*  callee,
                                             const lauf_asm_inst*      ip)
    {
        auto next_frame = frame_ptr->next_frame();

        if (auto cur_chunk = chunk::chunk_of(frame_ptr);
            LAUF_UNLIKELY(frame_ptr->next_offset + callee->max_cstack_size
                          > cur_chunk->remaining_space(next_frame)))
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

    void grow(page_allocator& alloc, void* frame_ptr)
    {
        auto cur_chunk  = chunk::chunk_of(frame_ptr);
        cur_chunk->next = chunk::allocate(alloc, page_allocator::page_size);
        _capacity += page_allocator::page_size;
    }

    std::size_t capacity() const
    {
        return _capacity;
    }

private:
    chunk*      _first    = nullptr;
    std::size_t _capacity = 0;
};
} // namespace lauf

//=== vstack ===//
namespace lauf
{
class vstack
{
public:
    void init(page_allocator& alloc, std::size_t initial_size)
    {
        _block = alloc.allocate(initial_size * sizeof(lauf_runtime_value));
    }

    void clear(page_allocator& alloc)
    {
        alloc.deallocate(_block);
    }

    lauf_runtime_value* base() const
    {
        // vstack grows down
        return reinterpret_cast<lauf_runtime_value*>(_block.ptr) + capacity();
    }
    lauf_runtime_value* limit() const
    {
        // vstack grows down
        // We keep a buffer of UINT8_MAX, so we can always call a builtin.
        return static_cast<lauf_runtime_value*>(_block.ptr) + UINT8_MAX;
    }

    std::size_t capacity() const
    {
        return _block.size / sizeof(lauf_runtime_value);
    }

    void grow(page_allocator& alloc, lauf_runtime_value*& vstack_ptr)
    {
        auto cur_size = std::size_t(base() - vstack_ptr);

        auto new_size  = 2 * _block.size;
        auto new_block = alloc.allocate(new_size);

        // We have filled [vstack_ptr, base) with cur_size values.
        // Need to copy them into [new_block.end - cur_size, new_block.end)
        auto dest = static_cast<lauf_runtime_value*>(new_block.ptr)
                    + new_block.size / sizeof(lauf_runtime_value) - cur_size;
        std::memcpy(dest, vstack_ptr, cur_size * sizeof(lauf_runtime_value));
        alloc.deallocate(_block);

        _block     = new_block;
        vstack_ptr = base() - cur_size;
    }

private:
    page_block _block;
};
} // namespace lauf

#endif // SRC_LAUF_RUNTIME_STACK_HPP_INCLUDED

