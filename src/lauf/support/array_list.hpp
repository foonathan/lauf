// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
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

    static constexpr auto block_size      = 1024 - 2 * sizeof(void*);
    static constexpr auto elems_per_block = block_size / sizeof(T);

    struct block
    {
        T      array[elems_per_block];
        block* next;
        block* prev;
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
        return size() == 0;
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
        ConstT* operator->() const
        {
            return &**this;
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

        _iterator& operator--()
        {
            if (_cur_idx == 0)
            {
                _cur_block = _cur_block->prev;
                _cur_idx   = elems_per_block - 1;
            }
            else
            {
                --_cur_idx;
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
        out += _next_idx;

        return out;
    }

    T& front()
    {
        return _first_block->array[0];
    }
    const T& front() const
    {
        return _first_block->array[0];
    }

    T& front(std::size_t idx)
    {
        return const_cast<T&>(static_cast<const array_list*>(this)->front(idx));
    }
    const T& front(std::size_t idx) const
    {
        if (LAUF_LIKELY(idx < elems_per_block))
            return _first_block->array[idx];

        auto block = _first_block;
        do
        {
            idx -= elems_per_block;
            assert(block);
            block = block->next;
        } while (idx >= elems_per_block);
        return block->array[idx];
    }

    T& back()
    {
        return _cur_block->array[_next_idx - 1];
    }
    const T& back() const
    {
        return _cur_block->array[_next_idx - 1];
    }

    T& back(std::size_t idx)
    {
        return const_cast<T&>(static_cast<const array_list*>(this)->back(idx));
    }
    const T& back(std::size_t idx) const
    {
        if (LAUF_LIKELY(idx < _next_idx))
            return _cur_block->array[_next_idx - idx - 1];

        auto block = _cur_block->prev;
        idx -= _next_idx;
        while (idx >= elems_per_block)
        {
            idx -= elems_per_block;
            assert(block);
            block = block->prev;
        }
        return block->array[elems_per_block - idx - 1];
    }

    //=== modifiers ===//
    T& push_back(arena_base& arena, const T& obj)
    {
        ensure_space(arena);

        auto result = ::new (&_cur_block->array[_next_idx]) T(obj);
        ++_next_idx;
        return *result;
    }

    template <typename... Args>
    T& emplace_back(arena_base& arena, Args&&... args)
    {
        ensure_space(arena);

        auto result = ::new (&_cur_block->array[_next_idx]) T(static_cast<Args&&>(args)...);
        ++_next_idx;
        return *result;
    }

    void pop_back()
    {
        if (_next_idx == 0)
        {
            _cur_block = _cur_block->prev;
            --_block_count;
            _next_idx = elems_per_block - 1;
        }
        else
        {
            --_next_idx;
        }
    }

    void reset()
    {
        _first_block = _cur_block = nullptr;
        _next_idx                 = 0;
        _block_count              = 0;
    }

private:
    void ensure_space(arena_base& arena)
    {
        if (_cur_block == nullptr)
        {
            assert(_first_block == nullptr);
            _first_block       = arena.template allocate<block>();
            _first_block->next = _first_block->prev = nullptr;
            _cur_block                              = _first_block;

            _next_idx = 0;
            ++_block_count;
        }
        else if (_next_idx == elems_per_block)
        {
            if (_cur_block->next == nullptr)
            {
                auto next        = arena.template allocate<block>();
                next->next       = nullptr;
                next->prev       = _cur_block;
                _cur_block->next = next;
            }
            _cur_block = _cur_block->next;

            _next_idx = 0;
            ++_block_count;
        }
    }

    block* get_front_block(std::size_t block_idx) const
    {
        assert(block_idx > 0);
        if (LAUF_LIKELY(block_idx == 0))
            return _first_block;

        auto cur = _first_block->next;
        while (--block_idx != 0)
        {
            assert(cur);
            cur = cur->next;
        }
        return cur;
    }
    block* get_back_block(std::size_t block_idx) const
    {
        if (LAUF_LIKELY(block_idx == 0))
            return _cur_block;

        auto cur = _cur_block->prev;
        while (--block_idx != 0)
        {
            assert(cur);
            cur = cur->prev;
        }
        return cur;
    }

    block*        _first_block;
    block*        _cur_block;
    std::uint32_t _next_idx;
    std::uint32_t _block_count;
};
} // namespace lauf

#endif // SRC_LAUF_SUPPORT_ARRAY_LIST_HPP_INCLUDED

