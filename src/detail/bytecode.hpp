// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_DETAIL_BYTECODE_HPP_INCLUDED
#define SRC_DETAIL_BYTECODE_HPP_INCLUDED

#include <climits>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <lauf/error.h>

namespace lauf
{
static_assert(CHAR_BIT == 8);

enum class op : unsigned char
{
    // push constant_index : uint16
    push,
    // pop count : uint16
    pop,
};

class bytecode_builder
{
public:
    void op(op o)
    {
        _bc.push_back(static_cast<unsigned char>(o));
    }

    void uint16(lauf_ErrorHandler& handler, lauf_ErrorContext ctx, std::size_t value)
    {
        _bc.emplace_back((value >> 8) & 0xFF);
        _bc.emplace_back((value >> 0) & 0xFF);

        if (value > UINT16_MAX)
        {
            handler.errors = true;
            handler.encoding_error(ctx, 16, value);
        }
    }

    std::size_t size() const
    {
        return _bc.size();
    }

    const unsigned char* data() const
    {
        return _bc.data();
    }

    void reset() &&
    {
        _bc.clear();
    }

private:
    std::vector<unsigned char> _bc;
};
} // namespace lauf

#define LAUF_BC_READ16(Ip) ((Ip) += 2, std::uint16_t((Ip)[-2] << 8 | (Ip)[-1]))

#endif // SRC_DETAIL_BYTECODE_HPP_INCLUDED

