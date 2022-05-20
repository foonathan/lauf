// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_DETAIL_STACK_CHECK_HPP_INCLUDED
#define SRC_DETAIL_STACK_CHECK_HPP_INCLUDED

#include <cstdint>
#include <lauf/detail/verify.hpp>

namespace lauf::_detail
{
class stack_checker
{
public:
    stack_checker() : _cur_size(0), _max_size(0) {}

    std::uint16_t cur_stack_size() const
    {
        return _cur_size;
    }
    std::uint16_t max_stack_size() const
    {
        return _max_size;
    }

    void set_stack_size(const char* instruction, std::size_t size)
    {
        _cur_size = 0;
        push(instruction, size);
    }

    void push(const char* instruction, std::size_t n = 1)
    {
        auto new_size = _cur_size + n;
        LAUF_VERIFY(new_size <= UINT16_MAX, instruction, "value stack size limit exceeded");

        _cur_size += n;
        if (_cur_size > _max_size)
            _max_size = _cur_size;
    }

    void pop(const char* instruction, std::size_t n = 1)
    {
        LAUF_VERIFY(_cur_size >= n, instruction, "missing stack values");
        _cur_size -= n;
    }

private:
    std::uint16_t _cur_size, _max_size;
};
} // namespace lauf::_detail

#endif // SRC_DETAIL_STACK_CHECK_HPP_INCLUDED

