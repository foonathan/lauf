// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_DETAIL_STACK_ALLOCATOR_HPP_INCLUDED
#define SRC_LAUF_DETAIL_STACK_ALLOCATOR_HPP_INCLUDED

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <lauf/config.h>
#include <lauf/value.h>
#include <type_traits>

namespace lauf::_detail
{
class stack_allocator
{
public:
    explicit stack_allocator(unsigned char* memory, std::size_t size)
    : _cur(memory), _begin(memory), _end(_cur + size)
    {}

    void* to_pointer(lauf_value_address addr) const
    {
        // TODO: check for invalid addresses
        return _begin + addr;
    }

    lauf_value_address to_address(const void* ptr) const
    {
        return static_cast<const unsigned char*>(ptr) - _begin;
    }

    const void* marker() const
    {
        return _cur;
    }
    void unwind(const void* marker)
    {
        _cur = (unsigned char*)(marker);
    }

    template <typename T>
    bool push(const T& object)
    {
        static_assert(std::is_trivially_copyable_v<T>);

        if (_cur + sizeof(T) > _end)
            return false;

        std::memcpy(_cur, &object, sizeof(T));
        _cur += sizeof(T);
        return true;
    }

    template <typename T>
    void pop(T& result)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        std::memcpy(&result, _cur -= sizeof(T), sizeof(T));
    }
    template <typename T>
    T pop()
    {
        T result;
        pop(result);
        return result;
    }

    void* allocate(std::size_t size, std::size_t alignment)
    {
        auto misaligned   = reinterpret_cast<std::uintptr_t>(_cur) & (alignment - 1);
        auto align_offset = misaligned != 0 ? (alignment - misaligned) : 0;
        if (_cur + align_offset + size > _end)
            return nullptr;

        _cur += align_offset;
        auto memory = _cur;
        _cur += size;
        return memory;
    }
    template <typename T>
    T* allocate(std::size_t n = 1)
    {
        return static_cast<T*>(allocate(n * sizeof(T), alignof(T)));
    }

private:
    unsigned char* _cur;
    unsigned char* _begin;
    unsigned char* _end;
};

class stack_allocator_layout
{
public:
    stack_allocator_layout(std::size_t initial_alignment = 0,
                           std::size_t max_size          = std::size_t(-1))
    : _impl(reinterpret_cast<unsigned char*>(initial_alignment), max_size) // NOLINT
    {}

    const void* marker() const
    {
        return _impl.marker();
    }
    void unwind(const void* marker)
    {
        _impl.unwind(marker);
    }

    std::size_t total_size() const
    {
        return _impl.to_address(marker());
    }

    lauf_value_address allocate(std::size_t size, std::size_t alignment)
    {
        auto memory = _impl.allocate(size, alignment);
        return _impl.to_address(memory);
    }

private:
    stack_allocator _impl;
};
} // namespace lauf::_detail

#endif // SRC_LAUF_DETAIL_STACK_ALLOCATOR_HPP_INCLUDED

