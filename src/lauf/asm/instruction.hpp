// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_ASM_INSTRUCTION_HPP_INCLUDED
#define SRC_LAUF_ASM_INSTRUCTION_HPP_INCLUDED

#include <lauf/config.h>

// The ASM instructions are also the bytecode for the VM.
// As such, there are many specializations and optimizations.
// It is also not designed to support edits; use the IR for that.

namespace lauf
{
enum class asm_op : std::uint8_t
{
#define LAUF_ASM_INST(Name, Type) Name,
#include "instruction.def.hpp"
#undef LAUF_ASM_INST
};

constexpr const char* to_string(asm_op op)
{
    switch (op)
    {
#define LAUF_ASM_INST(Name, Type)                                                                  \
case asm_op::Name:                                                                                 \
    return #Name;
#include "instruction.def.hpp"
#undef LAUF_ASM_INST
    }
}
} // namespace lauf

namespace lauf
{
struct asm_inst_none
{
    asm_op op;
};

struct asm_inst_offset
{
    asm_op       op : 8;
    std::int32_t offset : 24;
};

struct asm_inst_value
{
    asm_op        op : 8;
    std::uint32_t value : 24;
};

struct asm_inst_stack_idx
{
    asm_op        op;
    std::uint16_t idx;
};

union asm_inst
{
#define LAUF_ASM_INST(Name, Type) Type Name;
#include "instruction.def.hpp"
#undef LAUF_ASM_INST

    constexpr asm_inst() : nop{asm_op::nop}
    {
        static_assert(sizeof(asm_inst) == sizeof(std::uint32_t));
    }

    constexpr asm_op op() const
    {
        return nop.op;
    }
};
} // namespace lauf

#endif // SRC_LAUF_ASM_INSTRUCTION_HPP_INCLUDED

