// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_ASM_INSTRUCTION_HPP_INCLUDED
#define SRC_LAUF_ASM_INSTRUCTION_HPP_INCLUDED

#include <lauf/config.h>

#include <cassert>
#include <lauf/support/align.hpp>
#include <type_traits>

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
    count,
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

    case asm_op::count:
        assert(false);
        return nullptr;
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

template <typename CurType, typename DestType>
std::ptrdiff_t compress_pointer_offset(CurType* _cur, DestType* _dest)
{
    auto cur  = (void*)(_cur);
    auto dest = (void*)(_dest);
    assert(is_aligned(cur, alignof(void*)) && is_aligned(dest, alignof(void*)));
    return (void**)dest - (void**)cur;
}

template <typename DestType, typename CurType>
const DestType* uncompress_pointer_offset(CurType* cur, std::ptrdiff_t offset)
{
    return (const DestType*)(reinterpret_cast<void* const*>(cur) + offset);
}

struct asm_inst_signature
{
    asm_op       op;
    std::uint8_t input_count;
    std::uint8_t output_count;
    std::uint8_t flags;
};

struct asm_inst_layout
{
    asm_op        op;
    std::uint8_t  alignment_log2;
    std::uint16_t size;

    constexpr std::size_t alignment() const
    {
        return std::size_t(1) << alignment_log2;
    }
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

struct asm_inst_local_addr
{
    asm_op        op;
    std::uint8_t  index;
    std::uint16_t offset;
};
} // namespace lauf

union lauf_asm_inst
{
#define LAUF_ASM_INST(Name, Type) lauf::Type Name;
#include "instruction.def.hpp"
#undef LAUF_ASM_INST

    constexpr lauf_asm_inst() : nop{lauf::asm_op::nop}
    {
        static_assert(sizeof(lauf_asm_inst) == sizeof(std::uint32_t));
    }

    constexpr lauf::asm_op op() const
    {
        return nop.op;
    }
};

#endif // SRC_LAUF_ASM_INSTRUCTION_HPP_INCLUDED

