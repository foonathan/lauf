// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_SUPPORT_ARENA_HPP_INCLUDED
#define SRC_LAUF_SUPPORT_ARENA_HPP_INCLUDED

#include <cassert>
#include <cstring>
#include <lauf/config.h>
#include <lauf/support/align.hpp>
#include <new>

namespace lauf
{
/// Adds arena allocation to an object.
template <typename Derived>
class arena
{
    // This is also the maximum size that can be allocated in the arena.
    static constexpr auto block_size = std::size_t(16) * 1024 - sizeof(void*);

    struct block
    {
        unsigned char memory[block_size];
        block*        next;

        static block* allocate()
        {
            auto ptr  = new block; // don't initialize memory
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
    template <typename... Args>
    static Derived* create(Args&&... args)
    {
        // Allocate the first block.
        auto b = block::allocate();
        assert(is_aligned(b, alignof(Derived)));
        assert(b->memory == reinterpret_cast<unsigned char*>(b));

        // Construct ourself in its memory.
        return ::new (b->memory) Derived(static_cast<Args&&>(args)...);
    }

    static void destroy(Derived* derived)
    {
        auto b = static_cast<arena*>(derived)->first_block();
        derived->~Derived();
        block::deallocate(b);
    }

    arena(const arena&)            = delete;
    arena& operator=(const arena&) = delete;

    //=== allocation ===//
    void* allocate(std::size_t size, std::size_t alignment)
    {
        if (size > block_size)
            return nullptr;

        auto offset    = align_offset(_cur_pos, alignment);
        auto remaining = std::size_t(_cur_block->end() - _cur_pos);
        if (offset + size > remaining)
        {
            auto next        = block::allocate();
            _cur_block->next = next;

            _cur_block = next;
            _cur_pos   = &next->memory[0];
            offset     = 0;
        }

        _cur_pos += offset;
        auto result = _cur_pos;
        _cur_pos += size;
        return result;
    }

    template <typename T>
    T* allocate()
    {
        return static_cast<T*>(allocate(sizeof(T), alignof(T)));
    }

    template <typename T, typename... Args>
    T* construct(Args&&... args)
    {
        return ::new (allocate<T>()) T(static_cast<Args&&>(args)...);
    }

    const unsigned char* memdup(const void* memory, std::size_t size, std::size_t alignment = 1)
    {
        auto ptr = static_cast<unsigned char*>(allocate(size, alignment));
        assert(ptr);
        std::memcpy(ptr, memory, size);
        return ptr;
    }
    const char* strdup(const char* str)
    {
        auto ptr = static_cast<char*>(allocate(std::strlen(str) + 1, 1));
        assert(ptr);
        std::strcpy(ptr, str);
        return ptr;
    }

protected:
    arena() : _cur_block(first_block()), _cur_pos(&_cur_block->memory[0])
    {
        _cur_pos += sizeof(Derived);
    }

    ~arena()
    {
        auto cur = first_block()->next;
        while (cur != nullptr)
            cur = block::deallocate(cur);
    }

private:
    block* first_block()
    {
        // We are always in the beginning memory of the first block.
        return reinterpret_cast<block*>(static_cast<Derived*>(this));
    }

    block*         _cur_block;
    unsigned char* _cur_pos;
};
} // namespace lauf

#endif // SRC_LAUF_SUPPORT_ARENA_HPP_INCLUDED

