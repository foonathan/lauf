// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_SUPPORT_STACK_ALLOCATOR_HPP_INCLUDED
#define SRC_LAUF_SUPPORT_STACK_ALLOCATOR_HPP_INCLUDED

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <lauf/config.h>
#include <lauf/value.h>
#include <type_traits>

namespace lauf
{
constexpr std::size_t align_offset(std::uintptr_t address, std::size_t alignment)
{
    auto misaligned = address & (alignment - 1);
    return misaligned != 0 ? (alignment - misaligned) : 0;
}
inline std::size_t align_offset(const void* address, std::size_t alignment)
{
    return align_offset(reinterpret_cast<std::uintptr_t>(address), alignment);
}
} // namespace lauf

namespace lauf
{
class memory_stack
{
    static constexpr std::size_t block_size = std::size_t(16) * 1024 - sizeof(void*);

    struct block
    {
        block*        next;
        unsigned char memory[block_size];

        static block* allocate()
        {
            auto ptr  = new block;
            ptr->next = nullptr;
            return ptr;
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
    explicit memory_stack(std::size_t memory_limit) noexcept
    : _block_count(1), _limit(memory_limit / block_size)
    {
        assert(memory_limit >= block_size);
        _head.next = nullptr;
    }
    memory_stack() : memory_stack(block_size)
    {
        _limit = std::size_t(-1);
    }

    ~memory_stack() noexcept
    {
        reset();
    }

    memory_stack(const memory_stack& other) noexcept = delete;
    memory_stack& operator=(const memory_stack& other) noexcept = delete;

    void reset()
    {
        auto cur = _head.next;
        while (cur != nullptr)
            cur = block::deallocate(cur);
        _head.next   = nullptr;
        _block_count = 1;
    }

private:
    std::size_t _block_count, _limit;
    block       _head;

    friend class stack_allocator;
};

class stack_allocator
{
public:
    explicit stack_allocator(memory_stack& stack)
    : _cur_block(&stack._head), _cur_pos(&stack._head.memory[0]), _stack(&stack)
    {}

    //=== allocation ===//
    static constexpr std::size_t max_allocation_size()
    {
        return memory_stack::block_size;
    }

    bool reserve_new_block()
    {
        if (_cur_block->next == nullptr)
        {
            auto next        = memory_stack::block::allocate();
            _cur_block->next = next;

            ++_stack->_block_count;
            if (_stack->_block_count > _stack->_limit)
                return false;
        }

        _cur_block = _cur_block->next;
        _cur_pos   = &_cur_block->memory[0];
        return true;
    }

    template <std::size_t Alignment = 1>
    void* allocate(std::size_t size)
    {
        assert(size < max_allocation_size());
        auto offset = Alignment == 1 ? 0 : align_offset(_cur_pos, Alignment);
        if (remaining_capacity() < offset + size)
        {
            if (!reserve_new_block())
                return nullptr;
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
        memory_stack::block* _block;
        unsigned char*       _block_pos;
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

private:
    std::size_t remaining_capacity() const noexcept
    {
        return std::size_t(_cur_block->end() - _cur_pos);
    }

    memory_stack::block* _cur_block;
    unsigned char*       _cur_pos;
    memory_stack*        _stack;
};
} // namespace lauf

namespace lauf
{
// Computes offsets for allocations.
class stack_allocator_offset
{
public:
    constexpr stack_allocator_offset() : stack_allocator_offset(alignof(void*)) {}
    constexpr explicit stack_allocator_offset(std::size_t initial_alignment)
    : _begin(initial_alignment), _cur(_begin)
    {}

    constexpr std::size_t size() const
    {
        return std::size_t(_cur - _begin);
    }

    constexpr std::uintptr_t allocate(std::size_t size, std::size_t alignment)
    {
        _cur += align_offset(_cur, alignment);
        auto result = _cur - _begin;
        _cur += size;
        return result;
    }

    template <typename T>
    constexpr std::uintptr_t allocate(std::size_t count = 1)
    {
        return allocate(count * sizeof(T), alignof(T));
    }

    constexpr void align_to(std::size_t alignment)
    {
        _cur += align_offset(_cur, alignment);
    }

private:
    std::uintptr_t _begin;
    std::uintptr_t _cur;
};
} // namespace lauf

#endif // SRC_LAUF_SUPPORT_STACK_ALLOCATOR_HPP_INCLUDED

