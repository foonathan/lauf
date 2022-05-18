// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_DETAIL_STACK_CHECK_HPP_INCLUDED
#define SRC_DETAIL_STACK_CHECK_HPP_INCLUDED

#include <cstddef>
#include <lauf/detail/verify.hpp>

namespace lauf::_detail
{
class stack_checker
{
public:
    explicit stack_checker(lauf_signature sig)
    : _cur_size(sig.input_count), _max_size(_cur_size), _final_size(sig.output_count)
    {}

    std::size_t cur_stack_size() const
    {
        return _cur_size;
    }
    std::size_t max_stack_size() const
    {
        return _max_size;
    }

    void finish_jump(const char* instruction, lauf_signature next)
    {
        LAUF_VERIFY(_cur_size == _final_size, instruction, "invalid signature for block");
        LAUF_VERIFY(_final_size == next.input_count, instruction,
                    "cannot chain blocks with incompatible signatures");
    }
    void finish_return(const char* instruction, lauf_signature fn)
    {
        LAUF_VERIFY(_cur_size == _final_size, instruction, "invalid signature for block");
        LAUF_VERIFY(_final_size == fn.output_count, instruction,
                    "exit block signature does not match function signature");
    }

    void push(std::size_t n = 1)
    {
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
    std::size_t _cur_size, _max_size, _final_size;
};
} // namespace lauf::_detail

#endif // SRC_DETAIL_STACK_CHECK_HPP_INCLUDED

