// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_SUPPORT_VIRTUAL_MEMORY_HPP_INCLUDED
#define SRC_LAUF_SUPPORT_VIRTUAL_MEMORY_HPP_INCLUDED

#include <cstddef>
#include <cstring>
#include <lauf/config.h>
#include <lauf/support/align.hpp>

namespace lauf
{
struct virtual_memory
{
    unsigned char* ptr  = nullptr;
    std::size_t    size = 0;
};

virtual_memory allocate_executable_memory(std::size_t size);
void           free_executable_memory(virtual_memory memory);

/// May change the address.
virtual_memory resize_executable_memory(virtual_memory memory, std::size_t size);

/// Enables writing to the executable memory, but disables execution.
void lock_executable_memory(virtual_memory memory);
/// Disables writing to the executable, but enables execution again.
void unlock_executable_memory(virtual_memory memory);
} // namespace lauf

namespace lauf
{
enum class executable_memory_handle : std::size_t
{
};

constexpr executable_memory_handle null_executable_memory
    = executable_memory_handle(std::size_t(-1));

class executable_memory_allocator
{
public:
    constexpr executable_memory_allocator() : _memory{}, _pos(0) {}

    executable_memory_allocator(const executable_memory_allocator&) = delete;
    executable_memory_allocator& operator=(const executable_memory_allocator&) = delete;

    ~executable_memory_allocator() noexcept
    {
        free_executable_memory(_memory);
    }

    virtual_memory memory() const
    {
        return _memory;
    }

    template <typename T>
    T* deref(executable_memory_handle ptr) const
    {
        return reinterpret_cast<T*>(_memory.ptr + std::size_t(ptr));
    }

    template <std::size_t Alignment = 1>
    executable_memory_handle align()
    {
        auto offset = Alignment == 1 ? 0 : align_offset(_pos, Alignment);
        if (remaining_capacity() < offset)
            grow(_memory.size + offset);

        _pos += offset;
        return executable_memory_handle(_pos);
    }

    template <std::size_t Alignment = 1>
    executable_memory_handle allocate(std::size_t size)
    {
        auto offset = Alignment == 1 ? 0 : align_offset(_pos, Alignment);
        if (remaining_capacity() < offset + size)
            grow(_memory.size + offset + size);

        _pos += offset;
        auto ptr = _pos;
        _pos += size;
        return executable_memory_handle(ptr);
    }

    template <std::size_t Alignment = 1>
    executable_memory_handle allocate(const void* data, std::size_t size)
    {
        auto ptr = allocate<Alignment>(size);
        lock_executable_memory(_memory);
        std::memcpy(deref<void>(ptr), data, size);
        unlock_executable_memory(_memory);
        return ptr;
    }

    template <std::size_t Alignment = 1>
    void place(const void* data, std::size_t size)
    {
        auto expected_ptr = _pos;
        auto ptr          = allocate<Alignment>(size);
        assert(std::size_t(ptr) == expected_ptr);

        lock_executable_memory(_memory);
        std::memcpy(deref<void>(ptr), data, size);
        unlock_executable_memory(_memory);
    }

private:
    std::size_t remaining_capacity() const
    {
        return std::size_t(_memory.size - _pos);
    }

    void grow(std::size_t min_size_needed)
    {
        auto new_size = 2 * _memory.size;
        if (new_size < min_size_needed)
            new_size = min_size_needed;

        _memory = resize_executable_memory(_memory, new_size);
    }

    virtual_memory _memory;
    std::size_t    _pos;
};
} // namespace lauf

#endif // SRC_LAUF_SUPPORT_VIRTUAL_MEMORY_HPP_INCLUDED

