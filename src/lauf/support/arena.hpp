// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
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
class arena_key
{
    arena_key() = default;

    template <typename Derived>
    friend class intrinsic_arena;
};

/// An arena.
class arena_base
{
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

    struct extern_alloc
    {
        extern_alloc*    next;
        void*            allocation;
        std::align_val_t align;

        explicit extern_alloc(extern_alloc* next, void* allocation, std::align_val_t align)
        : next(next), allocation(allocation), align(align)
        {}
    };

public:
    arena_base(const arena_base&)            = delete;
    arena_base& operator=(const arena_base&) = delete;

    void* allocate(std::size_t size, std::size_t alignment)
    {
        if (LAUF_UNLIKELY(size > block_size))
        {
            auto memory = ::operator new(size, std::align_val_t(alignment));
            _extern_allocs
                = construct<extern_alloc>(_extern_allocs, memory, std::align_val_t(alignment));
            return memory;
        }

        auto offset    = align_offset(_cur_pos, alignment);
        auto remaining = std::size_t(_cur_block->end() - _cur_pos);
        if (LAUF_UNLIKELY(offset + size > remaining))
        {
            if (LAUF_UNLIKELY(_cur_block->next == nullptr))
                _cur_block->next = block::allocate();

            _cur_block = _cur_block->next;
            _cur_pos   = &_cur_block->memory[0];
            offset     = 0;
        }

        _cur_pos += offset;
        auto result = _cur_pos;
        _cur_pos += size;
        return result;
    }

    bool try_expand(void* ptr, std::size_t cur_size, std::size_t new_size)
    {
        auto end = static_cast<unsigned char*>(ptr) + cur_size;
        if (end != _cur_pos)
            return false;

        auto increase  = new_size - cur_size;
        auto remaining = std::size_t(_cur_block->end() - _cur_pos);
        if (remaining < new_size - cur_size)
            return false;

        _cur_pos += increase;
        return true;
    }

    template <typename T>
    T* allocate(std::size_t count = 1)
    {
        return static_cast<T*>(allocate(count * sizeof(T), alignof(T)));
    }
    template <typename T, typename... Args>
    T* construct(Args&&... args)
    {
        return ::new (allocate<T>()) T(static_cast<Args&&>(args)...);
    }

    unsigned char* memdup(const void* memory, std::size_t size, std::size_t alignment = 1)
    {
        auto ptr = static_cast<unsigned char*>(allocate(size, alignment));
        std::memcpy(ptr, memory, size);
        return ptr;
    }
    const char* strdup(const char* str)
    {
        return reinterpret_cast<const char*>(memdup(str, std::strlen(str) + 1));
    }

    struct marker
    {
        block*         cur_block;
        unsigned char* cur_pos;
    };

    marker get_marker() const
    {
        return {_cur_block, _cur_pos};
    }

    void unwind(marker m)
    {
        _cur_block = m.cur_block;
        _cur_pos   = m.cur_pos;
    }

private:
    explicit arena_base(block* first_block, std::size_t sizeof_derived)
    : _cur_block(first_block), _cur_pos(&_cur_block->memory[0]), _extern_allocs(nullptr)
    {
        _cur_pos += sizeof_derived;
    }

    // We can't destroy it, so parent needs to do it.
    ~arena_base() = default;

    void destroy_impl(block* first_block)
    {
        for (auto cur = _extern_allocs; cur != nullptr; cur = cur->next)
            ::operator delete(cur->allocation, cur->align);

        for (auto cur = first_block->next; cur != nullptr;)
            cur = block::deallocate(cur);
    }

    void clear_impl(block* first_block, std::size_t sizeof_derived)
    {
        // Deallocate all extern allocations.
        for (auto cur = _extern_allocs; cur != nullptr; cur = cur->next)
            ::operator delete(cur->allocation, cur->align);
        _extern_allocs = nullptr;

        // Reset the stack allocator to the beginning.
        // We don't free the blocks themselves, as we could re-use them.
        // We also don't touch the actual bytes of the derived object.
        _cur_block = first_block;
        _cur_pos   = &_cur_block->memory[0];
        _cur_pos += sizeof_derived;
    }

    block*         _cur_block;
    unsigned char* _cur_pos;
    extern_alloc*  _extern_allocs;

    template <typename Derived>
    friend class intrinsic_arena;
};

/// Adds arena allocation to an object.
template <typename Derived>
class intrinsic_arena : public arena_base
{
public:
    template <typename... Args>
    static Derived* create(Args&&... args)
    {
        // Allocate the first block.
        auto b = block::allocate();
        assert(is_aligned(b, alignof(Derived)));
        assert(b->memory == reinterpret_cast<unsigned char*>(b));

        // Construct ourself in its memory.
        return ::new (b->memory) Derived(arena_key(), static_cast<Args&&>(args)...);
    }

    static void destroy(Derived* derived)
    {
        auto b = static_cast<intrinsic_arena*>(derived)->first_block();
        derived->~Derived();
        block::deallocate(b);
    }

    void clear()
    {
        clear_impl(first_block(), sizeof(Derived));
    }

protected:
    intrinsic_arena(arena_key) : arena_base(first_block(), sizeof(Derived)) {}

    ~intrinsic_arena()
    {
        destroy_impl(first_block());
    }

private:
    block* first_block()
    {
        // We are always in the beginning memory of the first block.
        return reinterpret_cast<block*>(static_cast<Derived*>(this));
    }
};

struct arena : intrinsic_arena<arena>
{
    arena(arena_key key) : intrinsic_arena<arena>(key) {}
};
} // namespace lauf

#endif // SRC_LAUF_SUPPORT_ARENA_HPP_INCLUDED

