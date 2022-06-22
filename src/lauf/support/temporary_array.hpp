// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_DETAIL_TEMPORARY_ARRAY_HPP_INCLUDED
#define SRC_LAUF_DETAIL_TEMPORARY_ARRAY_HPP_INCLUDED

#include <cassert>
#include <cstddef>
#include <lauf/support/stack_allocator.hpp>

namespace lauf
{
template <typename T>
class temporary_array
{
    static_assert(std::is_trivially_copyable_v<T>, "unimplemented");

public:
    //=== constructors ===//
    temporary_array() : _data(nullptr), _size(0), _capacity(0) {}

    explicit temporary_array(stack_allocator& alloc, size_t expected_size)
    : _data(nullptr), _size(0), _capacity(uint32_t(expected_size))
    {
        if (_capacity * sizeof(T) <= alloc.max_allocation_size())
            _data = static_cast<T*>(alloc.allocate<alignof(T)>(expected_size * sizeof(T)));
        else
            _data = static_cast<T*>(::operator new(sizeof(T) * _capacity));
    }

    temporary_array(const temporary_array&) = delete;
    temporary_array& operator=(const temporary_array&) = delete;

    temporary_array(temporary_array&& other) noexcept
    : _data(other._data), _size(other._size), _capacity(other._capacity)
    {
        other._data = nullptr;
        other._size = other._capacity = 0;
    }
    temporary_array& operator=(temporary_array&& other) noexcept
    {
        if (_capacity * sizeof(T) > stack_allocator::max_allocation_size())
            ::operator delete(_data);

        _data     = other._data;
        _size     = other._size;
        _capacity = other._capacity;

        other._data = nullptr;
        other._size = other._capacity = 0;

        return *this;
    }

    ~temporary_array() noexcept
    {
        if (_capacity * sizeof(T) > stack_allocator::max_allocation_size())
            ::operator delete(_data);
    }

    //=== access ===//
    bool empty() const noexcept
    {
        return _size == 0;
    }
    std::size_t size() const noexcept
    {
        return _size;
    }

    T& operator[](std::size_t idx) noexcept
    {
        assert(idx < _size);
        return _data[idx];
    }
    const T& operator[](std::size_t idx) const noexcept
    {
        assert(idx < _size);
        return _data[idx];
    }

    T& front() noexcept
    {
        return _data[0];
    }
    const T& front() const noexcept
    {
        return _data[0];
    }

    T& back() noexcept
    {
        return _data[_size - 1];
    }
    const T& back() const noexcept
    {
        return _data[_size - 1];
    }

    T* data() noexcept
    {
        return _data;
    }
    const T* data() const noexcept
    {
        return _data;
    }

    T* begin() noexcept
    {
        return _data;
    }
    const T* begin() const noexcept
    {
        return _data;
    }

    T* end() noexcept
    {
        return _data + _size;
    }
    const T* end() const noexcept
    {
        return _data + _size;
    }

    //=== modifiers ===//
    void clear_and_reserve(stack_allocator& alloc, size_t capacity)
    {
        auto memory = alloc.allocate<alignof(T)>(capacity * sizeof(T));
        _data       = static_cast<T*>(memory);
        _size       = 0;
        _capacity   = capacity;
    }

    void push_back(const T& object)
    {
        assert(_size < _capacity);
        _data[_size] = object;
        ++_size;
    }
    void push_back(stack_allocator& alloc, const T& object)
    {
        if (_size == _capacity)
            grow(alloc);

        _data[_size] = object;
        ++_size;
    }

    template <typename... Args>
    void emplace_back(stack_allocator& alloc, Args&&... args)
    {
        if (_size == _capacity)
            grow(alloc);

        _data[_size] = T(static_cast<Args&&>(args)...);
        ++_size;
    }

    void resize(std::size_t n, const T& value = {})
    {
        if (n <= _size)
        {
            // No need to free anything.
            _size = n;
        }
        else
        {
            assert(n <= _capacity);
            for (; _size != n; ++_size)
                _data[_size] = value;
        }
    }
    void resize(stack_allocator& alloc, std::size_t n, const T& value = {})
    {
        if (n <= _size)
        {
            // No need to free anything.
            _size = n;
        }
        else
        {
            if (n > _capacity)
                grow(alloc);

            for (; _size != n; ++_size)
                _data[_size] = value;
        }
    }

    void pop_back()
    {
        --_size;
    }

private:
    void grow(stack_allocator& alloc)
    {
        constexpr auto default_capacity = 128;
        auto twice_capacity = _capacity == 0 ? default_capacity : 2 * std::size_t(_capacity);
        auto new_capacity   = twice_capacity < alloc.max_allocation_size()
                                  ? twice_capacity
                                  : alloc.max_allocation_size();
        assert(new_capacity > _size);

        auto new_memory = alloc.allocate<alignof(T)>(new_capacity * sizeof(T));
        std::memcpy(new_memory, _data, _size * sizeof(T));

        // We only free the previous allocation if it's from the heap (we can't with a stack
        // allocator). However, this is fine, as it's temporary storage.
        if (_capacity * sizeof(T) > stack_allocator::max_allocation_size())
            ::operator delete(_data);

        _data     = static_cast<T*>(new_memory);
        _capacity = new_capacity;
    }

    T*       _data;
    uint32_t _size;
    uint32_t _capacity;
};
} // namespace lauf

#endif // SRC_LAUF_DETAIL_TEMPORARY_ARRAY_HPP_INCLUDED

