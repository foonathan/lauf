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

constexpr auto UINT21_MAX = 0x1F'FFFF;
constexpr auto UINT24_MAX = 0xFF'FFFF;

// Conditions for conditional jumps.
enum class condition_code : unsigned char
{
    // Top value has all bits zero.
    if_zero = 0,
    // Top value has not all bits zero.
    if_nonzero = 1,

    // Top value as integer == 0.
    cmp_eq = if_zero,
    // Top value as integer != 0.
    cmp_ne = if_nonzero,
    // Top value as integer < 0.
    cmp_lt = 4,
    // Top value as integer <= 0.
    cmp_le = 5,
    // Top value as integer > 0.
    cmp_gt = 6,
    // Top value as integer >= 0.
    cmp_ge = 7
};

// Instruction is 32 bits.
// * op code are the least significant 8 bits
// * payload are the remaining 24 bits
enum class op : unsigned char
{
    // payload: none,
    return_,
    // payload: relative offset
    jump,
    // payload: condition code (3 bits) + relative offset (21 bits)
    jump_if,

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

    // payload: argument index
    argument,

    // payload: constant index
    pop,
    // payload: none
    pop_one,

    // payload: constant index for the function
    call,
    // payload: constant index for the function
    call_builtin,
};

#define LAUF_BC_OP(Inst) static_cast<lauf::op>(Inst)
#define LAUF_BC_PAYLOAD(Inst) ((Inst) >> 8)

using bytecode = const std::uint32_t*;

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

    std::size_t jump()
    {
        auto idx = _bc.size();
        _bc.push_back(static_cast<unsigned char>(op::jump));
        return idx;
    }
    void patch_jump(lauf_ErrorHandler& handler, lauf_ErrorContext ctx, std::size_t idx,
                    std::size_t dest)
    {
        auto offset = dest - idx;
        _bc[idx] |= (offset & UINT21_MAX) << 8;
        if (offset > UINT21_MAX)
        {
            handler.errors = true;
            handler.encoding_error(ctx, 24, offset);
        }
    }

    std::size_t jump_if(condition_code cc)
    {
        auto idx = _bc.size();
        _bc.push_back((static_cast<unsigned char>(cc) & 0b111) << 8
                      | static_cast<unsigned char>(op::jump_if));
        return idx;
    }
    void patch_jump_if(lauf_ErrorHandler& handler, lauf_ErrorContext ctx, std::size_t idx,
                       std::size_t dest)
    {
        auto offset = dest - idx;
        _bc[idx] |= (offset & UINT21_MAX) << 11;
        if (offset > UINT21_MAX)
        {
            handler.errors = true;
            handler.encoding_error(ctx, 21, offset);
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

