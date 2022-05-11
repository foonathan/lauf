// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_DETAIL_BYTECODE_HPP_INCLUDED
#define SRC_DETAIL_BYTECODE_HPP_INCLUDED

#include <lauf/config.h>
#include <lauf/detail/verify.hpp>

namespace lauf::_detail
{
enum class bc_op : uint8_t
{
#define LAUF_BC_OP(Name, Type) Name,
#include "bc_ops.h"
#undef LAUF_BC_OP
};

inline const char* to_string(bc_op op)
{
    switch (op)
    {
#define LAUF_BC_OP(Name, Type)                                                                     \
case bc_op::Name:                                                                                  \
    return #Name;
#include "bc_ops.h"
#undef LAUF_BC_OP
    }
}

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

struct bc_inst_none
{
    bc_op    op : 8;
    uint32_t _padding : 24;

    bc_inst_none() : op{} {}
    explicit bc_inst_none(bc_op op) : op(op) {}
};

struct bc_inst_literal
{
    bc_op    op : 8;
    uint32_t literal : 24;

    template <typename T>
    explicit bc_inst_literal(bc_op op, T v) : op(op), literal(uint32_t(v))
    {
        LAUF_VERIFY(literal == v, to_string(op), "encoding error");
    }
};

enum class bc_literal_idx : uint32_t
{
};

struct bc_inst_literal_idx
{
    bc_op          op : 8;
    bc_literal_idx literal_idx : 16;
    uint32_t       _padding : 8;

    explicit bc_inst_literal_idx(bc_op op, bc_literal_idx idx) : op(op), literal_idx(idx)
    {
        LAUF_VERIFY(literal_idx == idx, to_string(op), "encoding error");
    }
};

struct bc_inst_field_literal_idx
{
    bc_op          op : 8;
    uint32_t       field : 8;
    bc_literal_idx literal_idx : 16;

    explicit bc_inst_field_literal_idx(bc_op op, size_t f, bc_literal_idx idx)
    : op(op), field(f), literal_idx(idx)
    {
        LAUF_VERIFY(field == f, to_string(op), "encoding error");
        LAUF_VERIFY(literal_idx == idx, to_string(op), "encoding error");
    }
};

enum class bc_function_idx : uint32_t
{
};

struct bc_inst_function_idx
{
    bc_op           op : 8;
    bc_function_idx function_idx : 24;

    explicit bc_inst_function_idx(bc_op op, bc_function_idx idx) : op(op), function_idx(idx)
    {
        LAUF_VERIFY(function_idx == idx, to_string(op), "encoding error");
    }
};

struct bc_inst_offset
{
    bc_op   op : 8;
    int32_t offset : 24;

    explicit bc_inst_offset(bc_op op, ptrdiff_t o) : op(op), offset(static_cast<int32_t>(o))
    {
        LAUF_VERIFY(offset == o, to_string(op), "encoding error");
    }
};

struct bc_inst_cc_offset
{
    bc_op          op : 8;
    condition_code cc : 3;
    int32_t        offset : 21;

    explicit bc_inst_cc_offset(bc_op op, condition_code cc, ptrdiff_t o)
    : op(op), cc(cc), offset(static_cast<int32_t>(o))
    {
        LAUF_VERIFY(offset == o, to_string(op), "encoding error");
    }
};

struct bc_inst_cc
{
    bc_op          op : 8;
    condition_code cc : 3;
    int32_t        _padding : 21;

    explicit bc_inst_cc(bc_op op, condition_code cc) : op(op), cc(cc) {}
};

union bc_instruction
{
    bc_inst_none tag;

#define LAUF_BC_OP(Name, Type) Type Name;
#include "bc_ops.h"
#undef LAUF_BC_OP

    bc_instruction() : tag() {}

    friend bool operator==(bc_instruction lhs, bc_instruction rhs)
    {
        return lhs.tag.op == rhs.tag.op && uint32_t(lhs.tag._padding) == uint32_t(rhs.tag._padding);
    }
    friend bool operator!=(bc_instruction lhs, bc_instruction rhs)
    {
        return !(lhs == rhs);
    }
};
static_assert(sizeof(bc_instruction) == sizeof(uint32_t));

#define LAUF_BC_INSTRUCTION(Op, ...)                                                               \
    [&] {                                                                                          \
        lauf::_detail::bc_instruction inst;                                                        \
        inst.Op = decltype(inst.Op){lauf::_detail::bc_op::Op, __VA_ARGS__};                        \
        return inst;                                                                               \
    }()
} // namespace lauf::_detail

#endif // SRC_DETAIL_BYTECODE_HPP_INCLUDED

