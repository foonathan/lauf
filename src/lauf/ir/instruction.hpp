// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_IR_INSTRUCTION_HPP_INCLUDED
#define SRC_LAUF_IR_INSTRUCTION_HPP_INCLUDED

#include <cstdint>
#include <lauf/bc/bytecode.hpp>
#include <lauf/builtin.h>
#include <lauf/config.h>
#include <lauf/module.h>
#include <lauf/value.h>

namespace lauf
{
enum class ir_op : std::uint8_t
{
#define LAUF_IR_OP(Name, Type) Name,
#include "ir_ops.h"
#undef LAUF_IR_OP
};
} // namespace lauf

namespace lauf
{
#define LAUF_IR_COMMON                                                                             \
    ir_op        op;                                                                               \
    std::uint8_t uses = 1

struct ir_inst_none
{
    LAUF_IR_COMMON;

    ir_inst_none(ir_op op) : op(op) {}
};

enum class param_idx : std::uint16_t
{
};
enum class register_idx : std::uint16_t
{
};
enum class block_idx : std::uint16_t
{
};

template <typename T>
struct ir_inst_index
{
    LAUF_IR_COMMON;
    T index;

    ir_inst_index(ir_op op, T index) : op(op), index(index) {}
};

struct ir_inst_value
{
    LAUF_IR_COMMON;
    lauf_value value;

    ir_inst_value(ir_op op, lauf_value value) : op(op), value(value) {}
};

struct ir_inst_return
{
    LAUF_IR_COMMON;
    std::uint8_t argument_count;

    ir_inst_return(ir_op op, std::uint8_t argument_count) : op(op), argument_count(argument_count)
    {}
};

struct ir_inst_jump
{
    LAUF_IR_COMMON;
    std::uint8_t argument_count;
    block_idx    dest;

    ir_inst_jump(ir_op op, std::uint8_t argument_count, block_idx dest)
    : op(op), argument_count(argument_count), dest(dest)
    {}
};

struct ir_inst_branch
{
    LAUF_IR_COMMON;
    std::uint8_t   argument_count;
    condition_code cc;
    register_idx   reg;
    block_idx      if_true, if_false;

    ir_inst_branch(ir_op op, std::size_t argument_count, register_idx reg, condition_code cc,
                   block_idx if_true, block_idx if_false)
    : op(op), argument_count(argument_count), cc(cc), reg(reg), if_true(if_true), if_false(if_false)
    {}
};

struct ir_inst_call_builtin
{
    LAUF_IR_COMMON;
    lauf_signature         signature;
    lauf_builtin_function* fn;

    ir_inst_call_builtin(ir_op op, lauf_signature signature, lauf_builtin_function fn)
    : op(op), signature(signature), fn(fn)
    {}
};
struct ir_inst_call
{
    LAUF_IR_COMMON;
    lauf_signature signature;
    lauf_function  fn;

    ir_inst_call(ir_op op, lauf_signature signature, lauf_function fn)
    : op(op), signature(signature), fn(fn)
    {}
};

struct ir_inst_argument
{
    LAUF_IR_COMMON;
    bool is_constant;
    union
    {
        lauf_value         constant;
        lauf::register_idx register_idx;
    };

    ir_inst_argument(ir_op op, lauf_value constant) : op(op), is_constant(true), constant(constant)
    {}
    ir_inst_argument(ir_op op, lauf::register_idx register_idx)
    : op(op), is_constant(false), register_idx(register_idx)
    {}
};

struct ir_inst_store_value
{
    LAUF_IR_COMMON;
    lauf::register_idx register_idx;
    std::uint32_t      local_addr;

    ir_inst_store_value(ir_op op, lauf::register_idx register_idx, std::uint32_t local_addr)
    : op(op), register_idx(register_idx), local_addr(local_addr)
    {}
};
struct ir_inst_load_value
{
    LAUF_IR_COMMON;
    std::uint32_t local_addr;

    ir_inst_load_value(ir_op op, std::uint32_t local_addr) : op(op), local_addr(local_addr) {}
};

union ir_inst
{
    ir_inst_none tag;
#define LAUF_IR_OP(Name, Type) lauf::Type Name;
#include "ir_ops.h"
#undef LAUF_IR_OP

    ir_inst() : tag({}) {}
};
static_assert(sizeof(ir_inst) == 2 * sizeof(lauf_value));

#define LAUF_IR_INSTRUCTION(Op, ...)                                                               \
    [&](auto... args) {                                                                            \
        lauf::ir_inst inst;                                                                        \
        inst.Op = decltype(inst.Op){lauf::ir_op::Op, args...};                                     \
        return inst;                                                                               \
    }(__VA_ARGS__)
} // namespace lauf

#endif // SRC_LAUF_IR_INSTRUCTION_HPP_INCLUDED

