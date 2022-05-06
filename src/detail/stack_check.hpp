// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_DETAIL_STACK_CHECK_HPP_INCLUDED
#define SRC_DETAIL_STACK_CHECK_HPP_INCLUDED

#include <cstddef>

namespace lauf::_detail
{
class stack_checker
{
public:
    stack_checker() : _cur_size(0), _max_size(0) {}

    void reset() &&
    {
        *this = {};
    }

    std::size_t cur_stack_size() const
    {
        return _cur_size;
    }
    std::size_t max_stack_size() const
    {
        return _max_size;
    }

    void push(std::size_t n = 1)
    {
        _cur_size += n;
        if (_cur_size > _max_size)
            _max_size = _cur_size;
    }

    bool pop(std::size_t n = 1)
    {
        if (_cur_size < n)
            return false;

        _cur_size -= n;
        return true;
    }

private:
    std::size_t _cur_size, _max_size;
};
} // namespace lauf::_detail

#endif // SRC_DETAIL_STACK_CHECK_HPP_INCLUDED

