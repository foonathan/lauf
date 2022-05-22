// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_DETAIL_STACK_ALLOCATOR_HPP_INCLUDED
#define SRC_LAUF_DETAIL_STACK_ALLOCATOR_HPP_INCLUDED

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <lauf/config.h>
#include <lauf/value.h>
#include <type_traits>

namespace lauf::_detail
{
inline std::size_t align_offset(std::uintptr_t address, std::size_t alignment)
{
    auto misaligned = address & (alignment - 1);
    return misaligned != 0 ? (alignment - misaligned) : 0;
}
inline std::size_t align_offset(const void* address, std::size_t alignment)
{
    return align_offset(reinterpret_cast<std::uintptr_t>(address), alignment);
}
} // namespace lauf::_detail

namespace lauf::_detail
{
class stack_allocator
{
    static constexpr std::size_t block_size = std::size_t(16) * 1024 - sizeof(void*);

    struct block
    {
        block*        next;
        unsigned char memory[block_size];

        block() : next(nullptr) // Don't initialize array
        {}

        static block* allocate()
        {
            return new block;
        }

        static block* deallocate(block* ptr)
        {
            auto next = ptr->next;
            delete ptr;
            return next;
        }

        unsigned char* end() noexcept
        {
            return &memory[block_size];
        }
    };

public:
    //=== constructors/destructors/assignment ===//
    stack_allocator() noexcept : _cur_block(nullptr), _cur_pos(nullptr)
    {
        reset();
    }

    ~stack_allocator() noexcept
    {
        auto cur = _head.next;
        while (cur != nullptr)
            cur = block::deallocate(cur);
    }

    stack_allocator(const stack_allocator& other) noexcept = delete;
    stack_allocator& operator=(const stack_allocator& other) noexcept = delete;

    //=== allocation ===//
    static constexpr std::size_t max_allocation_size()
    {
        return block_size;
    }

    void reserve_new_block()
    {
        if (_cur_block->next == nullptr)
        {
            auto next        = block::allocate();
            _cur_block->next = next;
        }

        _cur_block = _cur_block->next;
        _cur_pos   = &_cur_block->memory[0];
    }

    template <std::size_t Alignment = 1>
    void* allocate(std::size_t size)
    {
        assert(size < max_allocation_size());
        auto offset = Alignment == 1 ? 0 : align_offset(_cur_pos, Alignment);
        if (remaining_capacity() < offset + size)
        {
            reserve_new_block();
            offset = 0;
        }

        _cur_pos += offset;
        auto memory = _cur_pos;
        _cur_pos += size;
        return memory;
    }

    //=== unwinding ===//
    struct marker
    {
        block*         _block;
        unsigned char* _block_pos;
    };

    marker top() const
    {
        return {_cur_block, _cur_pos};
    }

    void unwind(marker m) noexcept
    {
        _cur_block = m._block;
        _cur_pos   = m._block_pos;
    }

    // Unwinds and frees unused blocks.
    void reset()
    {
        auto cur = _head.next;
        while (cur != nullptr)
            cur = block::deallocate(cur);

        _cur_block = &_head;
        _cur_pos   = &_cur_block->memory[0];
    }

private:
    std::size_t remaining_capacity() const noexcept
    {
        return std::size_t(_cur_block->end() - _cur_pos);
    }

    block*         _cur_block;
    unsigned char* _cur_pos;
    block          _head;
};
} // namespace lauf::_detail

namespace lauf::_detail
{
// Computes offsets for allocations.
class stack_allocator_offset
{
public:
    stack_allocator_offset() : stack_allocator_offset(alignof(void*)) {}
    explicit stack_allocator_offset(std::size_t initial_alignment)
    : _begin(initial_alignment), _cur(_begin)
    {}

    std::size_t size() const
    {
        return std::size_t(_cur - _begin);
    }

    std::uintptr_t allocate(std::size_t size, std::size_t alignment)
    {
        _cur += align_offset(_cur, alignment);
        auto result = _cur - _begin;
        _cur += size;
        return result;
    }

    template <typename T>
    std::uintptr_t allocate(std::size_t count = 1)
    {
        return allocate(count * sizeof(T), alignof(T));
    }

    void align_to(std::size_t alignment)
    {
        _cur += align_offset(_cur, alignment);
    }

private:
    std::uintptr_t _begin;
    std::uintptr_t _cur;
};
} // namespace lauf::_detail

#endif // SRC_LAUF_DETAIL_STACK_ALLOCATOR_HPP_INCLUDED

