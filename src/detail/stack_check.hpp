// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_DETAIL_STACK_CHECK_HPP_INCLUDED
#define SRC_DETAIL_STACK_CHECK_HPP_INCLUDED

#include <cstddef>
#include <cstdio>
#include <lauf/value.h>
#include <vector>

namespace lauf
{
class stack_checker
{
public:
    stack_checker() : _max_size(0), _errors(false) {}

    void reset() &&
    {
        _max_size = 0;
        _errors   = false;
        _stack.clear();
    }

    explicit operator bool() const
    {
        return !_errors;
    }

    std::size_t cur_stack_size() const
    {
        return _stack.size();
    }
    std::size_t max_stack_size() const
    {
        return _max_size;
    }

    void push(lauf_ValueType type, std::size_t n = 1)
    {
        for (auto i = std::size_t(0); i != n; ++i)
            _stack.push_back(type);
        _max_size = std::max(_stack.size(), _max_size);
    }
    void push_unknown(std::size_t n = 1)
    {
        push(lauf_ValueType(-1), n);
    }

    void pop(std::size_t n)
    {
        if (_stack.size() < n)
        {
            std::fprintf(stderr,
                         "stack error: Attempt to pop %zu value(s) from stack of size %zu.\n", n,
                         _stack.size());
            _errors = true;
            return;
        }

        _stack.erase(_stack.end() - n, _stack.end());
    }

private:
    std::size_t                 _max_size;
    std::vector<lauf_ValueType> _stack;
    bool                        _errors;
};
} // namespace lauf

#endif // SRC_DETAIL_STACK_CHECK_HPP_INCLUDED

