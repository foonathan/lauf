// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_SUPPORT_ARRAY_LIST_HPP_INCLUDED
#define SRC_LAUF_SUPPORT_ARRAY_LIST_HPP_INCLUDED

#include <lauf/config.h>
#include <lauf/support/arena.hpp>
#include <type_traits>

namespace lauf
{
/// Essentially a `std::forward_list<std::array<T, N>>` that uses an arena.
template <typename T>
class array_list
{
    static_assert(std::is_trivially_copyable_v<T>); // For simplicity for now.

    static constexpr auto block_size      = 1024 - sizeof(void*);
    static constexpr auto elems_per_block = block_size / sizeof(T);

    struct block
    {
        T      array[elems_per_block];
        block* next = nullptr;
    };

public:
    //=== constructors ===//
    array_list() : _first_block(nullptr), _cur_block(nullptr), _next_idx(0), _block_count(0) {}

    // For simplicity for now.
    array_list(const array_list&)            = delete;
    array_list& operator=(const array_list&) = delete;

    // Arena takes care of deallaction.
    ~array_list() = default;

    //=== access ===//
    bool empty() const
    {
        return _block_count == 0 && _next_idx == 0;
    }
    std::size_t size() const
    {
        if (_block_count == 0)
            return 0;
        else
            return (_block_count - 1) * elems_per_block + _next_idx;
    }

    template <typename ConstT>
    struct _iterator
    {
        block*      _cur_block;
        std::size_t _cur_idx;

        ConstT& operator*() const
        {
            return _cur_block->array[_cur_idx];
        }

        _iterator& operator++()
        {
            ++_cur_idx;
            if (_cur_idx == elems_per_block)
            {
                _cur_block = _cur_block->next;
                _cur_idx   = 0;
            }
            return *this;
        }

        friend bool operator==(_iterator lhs, _iterator rhs)
        {
            return lhs._cur_block == rhs._cur_block && lhs._cur_idx == rhs._cur_idx;
        }
        friend bool operator!=(_iterator lhs, _iterator rhs)
        {
            return !(lhs == rhs);
        }
    };
    using iterator       = _iterator<T>;
    using const_iterator = _iterator<const T>;

    iterator begin()
    {
        return {_first_block, 0};
    }
    iterator end()
    {
        if (_first_block == nullptr || _next_idx == elems_per_block)
            return {nullptr, 0};
        else
            return {_cur_block, _next_idx};
    }

    const_iterator begin() const
    {
        return {_first_block, 0};
    }
    const_iterator end() const
    {
        if (_first_block == nullptr || _next_idx == elems_per_block)
            return {nullptr, 0};
        else
            return {_cur_block, _next_idx};
    }

    T* copy_to(T* out)
    {
        if (empty())
            return out;

        for (auto block = _first_block; block != _cur_block; block = block->next)
        {
            std::memcpy(out, block->array, elems_per_block * sizeof(T));
            out += elems_per_block;
        }

        std::memcpy(out, _cur_block->array, _next_idx * sizeof(T));
        out += _next_idx * sizeof(T);

        return out;
    }

    //=== modifiers ===//
    template <typename Arena>
    T& push_back(Arena& arena, const T& obj)
    {
        ensure_space(arena);

        auto result = ::new (&_cur_block->array[_next_idx]) T(obj);
        ++_next_idx;
        return *result;
    }

    template <typename Arena, typename... Args>
    T& emplace_back(Arena& arena, Args&&... args)
    {
        ensure_space(arena);

        auto result = ::new (&_cur_block->array[_next_idx]) T(static_cast<Args&&>(args)...);
        ++_next_idx;
        return *result;
    }

    void clear()
    {
        _first_block = _cur_block = nullptr;
        _next_idx                 = 0;
        _block_count              = 0;
    }

private:
    template <typename Arena>
    void ensure_space(Arena& arena)
    {
        if (_cur_block == nullptr)
        {
            assert(_first_block == nullptr);
            _first_block = arena.template allocate<block>();
            _cur_block   = _first_block;

            _next_idx = 0;
            ++_block_count;
        }
        else if (_next_idx == elems_per_block)
        {
            auto next        = arena.template allocate<block>();
            _cur_block->next = next;
            _cur_block       = next;

            _next_idx = 0;
            ++_block_count;
        }
    }

    block*        _first_block;
    block*        _cur_block;
    std::uint32_t _next_idx;
    std::uint32_t _block_count;
};
} // namespace lauf

#endif // SRC_LAUF_SUPPORT_ARRAY_LIST_HPP_INCLUDED

