// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_DETAIL_STACK_CHECK_HPP_INCLUDED
#define SRC_DETAIL_STACK_CHECK_HPP_INCLUDED

#include <cstddef>
#include <lauf/error.h>

namespace lauf
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

    void pop(lauf_ErrorHandler& handler, lauf_ErrorContext ctx, std::size_t n)
    {
        if (_cur_size < n)
        {
            handler.errors = true;
            handler.stack_underflow(ctx, _cur_size, n);
            _cur_size = 0;
        }
        else
        {
            _cur_size -= n;
        }
    }

    void assert_empty(lauf_ErrorHandler& handler, lauf_ErrorContext ctx)
    {
        if (_cur_size > 0)
        {
            handler.errors = true;
            handler.stack_nonempty(ctx, _cur_size);
        }
        _cur_size = 0;
    }

private:
    std::size_t _cur_size, _max_size;
};
} // namespace lauf

#endif // SRC_DETAIL_STACK_CHECK_HPP_INCLUDED

