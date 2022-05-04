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

constexpr auto UINT24_MAX = 0xFF'FFFF;

// Instruction is 32 bits.
// * op code are the least significant 8 bits
// * payload are the remaining 24 bits
enum class op : unsigned char
{
    // payload: constant index
    push,
    // payload: none
    push_zero,
    // payload: constant
    // Pushes a 24 bit constant by extending it with zeroes.
    push_small_zext,
    // payload: constant
    // Pushes a 24 bit constant by extending it with zeroes and then negating it.
    push_small_neg,

    // payload: constant index
    pop,
    // payload: none
    pop_one,
};

#define LAUF_BC_OP(Inst) static_cast<lauf::op>(Inst)
#define LAUF_BC_PAYLOAD24(Inst) ((Inst) >> 8)

class bytecode_builder
{
public:
    void op(enum op o)
    {
        _bc.push_back(static_cast<unsigned char>(o));
    }

    void op(lauf_ErrorHandler& handler, lauf_ErrorContext ctx, enum op o, std::size_t payload)
    {
        _bc.push_back((payload & UINT24_MAX) << 8 | static_cast<unsigned char>(o));
        if (payload > UINT24_MAX)
        {
            handler.errors = true;
            handler.encoding_error(ctx, 24, payload);
        }
    }

    std::size_t size() const
    {
        return _bc.size();
    }

    const std::uint32_t* data() const
    {
        return _bc.data();
    }

    void reset() &&
    {
        _bc.clear();
    }

private:
    std::vector<std::uint32_t> _bc;
};
} // namespace lauf

#endif // SRC_DETAIL_BYTECODE_HPP_INCLUDED

