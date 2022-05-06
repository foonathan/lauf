// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_DETAIL_BYTECODE_HPP_INCLUDED
#define SRC_DETAIL_BYTECODE_HPP_INCLUDED

#include <lauf/config.h>
#include <lauf/error.h>

namespace lauf::_detail
{
enum class bc_op : uint8_t
{
#define LAUF_BC_OP(Name, Type) Name,
#include "bc_ops.h"
#undef LAUF_BC_OP
};

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

enum class bc_constant_idx : uint32_t
{
};

struct bc_inst_none
{
    bc_op    op : 8;
    uint32_t _padding : 24;
};

struct bc_inst_constant
{
    bc_op    op : 8;
    uint32_t constant : 24;
};

struct bc_inst_constant_idx
{
    bc_op           op : 8;
    bc_constant_idx constant_idx : 24;
};

struct bc_inst_offset
{
    bc_op   op : 8;
    int32_t offset : 24;
};

struct bc_inst_cc_offset
{
    bc_op          op : 8;
    condition_code cc : 3;
    int32_t        offset : 21;
};

union bc_instruction
{
    bc_inst_none tag;

#define LAUF_BC_OP(Name, Type) Type Name;
#include "bc_ops.h"
#undef LAUF_BC_OP
};
static_assert(sizeof(bc_instruction) == sizeof(uint32_t));

#define LAUF_BC_INSTRUCTION(Op, ...)                                                               \
    [&] {                                                                                          \
        lauf::_detail::bc_instruction inst;                                                        \
        inst.Op = {lauf::_detail::bc_op::Op, __VA_ARGS__};                                         \
        return inst;                                                                               \
    }()
} // namespace lauf::_detail

#endif // SRC_DETAIL_BYTECODE_HPP_INCLUDED

