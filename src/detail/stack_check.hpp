// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_DETAIL_STACK_CHECK_HPP_INCLUDED
#define SRC_DETAIL_STACK_CHECK_HPP_INCLUDED

#include <cstddef>
#include <cstdio>

namespace lauf
{
class stack_checker
{
public:
    stack_checker() : _cur_size(0), _max_size(0), _errors(false) {}

    void reset() &&
    {
        *this = {};
    }

    explicit operator bool() const
    {
        return !_errors;
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

    void pop(std::size_t n)
    {
        if (_cur_size < n)
        {
            std::fprintf(stderr,
                         "stack error: Attempt to pop %zu value(s) from stack of size %zu.\n", n,
                         _cur_size);
            _errors = true;
            return;
        }

        _cur_size -= n;
    }

private:
    std::size_t _cur_size, _max_size;
    bool        _errors;
};
} // namespace lauf

#endif // SRC_DETAIL_STACK_CHECK_HPP_INCLUDED

