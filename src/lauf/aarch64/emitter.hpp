// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_AARCH64_EMITTER_HPP_INCLUDED
#define SRC_LAUF_AARCH64_EMITTER_HPP_INCLUDED

#include <cstdint>
#include <cstring>
#include <lauf/config.h>
#include <lauf/literal_pool.hpp>
#include <vector>

namespace lauf::aarch64
{
enum class register_ : std::uint8_t
{
    result = 0,

    panic_result = 9,

    // For emitter use only.
    _scratch = 17,
};

constexpr std::uint8_t encode(register_ reg)
{
    return std::uint8_t(reg) & 0b11111;
}

enum class condition_code : std::uint8_t
{
    eq = 0b0000,
    ne = 0b0001,

    ge = 0b1010,
    lt = 0b1011,
    gt = 0b1100,
    le = 0b1101,
};

class emitter
{
public:
    //=== control flow ===//
    enum class label : std::uint16_t
    {
    };

    label declare_label()
    {
        auto idx = _labels.size();
        _labels.push_back(UINT32_MAX);
        return label(idx);
    }

    void place_label(label l)
    {
        _labels[std::size_t(l)] = _inst.size();
    }

    void ret()
    {
        // RET X30
        _inst.push_back(0b1101011'0'0'10'11111'0000'0'0'11110'00000);
    }

    void b(std::int32_t offset)
    {
        auto inst = 0b0'00101 << 26;
        inst |= offset & ((1 << 26) - 1);
        _inst.push_back(inst);
    }
    void b(label l)
    {
        if (auto pos = _labels[std::size_t(l)]; pos != UINT32_MAX)
        {
            b(std::int32_t(pos) - std::int32_t(_inst.size()));
        }
        else
        {
            _patches.push_back(make_patch(_inst.size(), branch_kind::unconditional));
            b(std::int32_t(l));
        }
    }
    void b_cond(label l, condition_code cond)
    {
        _patches.push_back(make_patch(_inst.size(), branch_kind::conditional));

        // B.cond l
        auto inst = std::uint32_t(0b0101010'0 << 24);
        inst |= std::uint32_t(l) << 5;
        inst |= std::uint8_t(cond) << 0;
        _inst.push_back(inst);
    }

    void cbz(register_ r, label l)
    {
        _patches.push_back(make_patch(_inst.size(), branch_kind::conditional));

        // CBZ r, l
        auto inst = std::uint32_t(0b1'011010'0 << 24);
        inst |= std::uint32_t(l) << 5;
        inst |= encode(r);
        _inst.push_back(inst);
    }
    void cbnz(register_ r, label l)
    {
        _patches.push_back(make_patch(_inst.size(), branch_kind::conditional));

        // CBNZ r, l
        auto inst = std::uint32_t(0b1'011010'1 << 24);
        inst |= std::uint32_t(l) << 5;
        inst |= encode(r);
        _inst.push_back(inst);
    }

    void recurse()
    {
        auto offset = (-std::int32_t(_inst.size())) & ((1 << 26) - 1);

        // BL offset
        auto inst = 0b1'00101 << 26;
        inst |= offset;
        _inst.push_back(inst);
    }

    template <typename Fn>
    void call(Fn* fn)
    {
        static_assert(std::is_function_v<Fn> && sizeof(fn) == sizeof(std::uint64_t));

        mov_imm(register_::_scratch, reinterpret_cast<std::uintptr_t>(fn));
        // BLR scratch
        _inst.push_back(0b1101011'0'0'01'11111'0000'0'0'00000'00000
                        | encode(register_::_scratch) << 5);
    }

    template <typename Fn>
    void tail_call(Fn* fn)
    {
        static_assert(std::is_function_v<Fn> && sizeof(fn) == sizeof(std::uint64_t));

        mov_imm(register_::_scratch, reinterpret_cast<std::uintptr_t>(fn));
        // BR scratch
        _inst.push_back(0b1101011'0'0'00'11111'0000'0'0'00000'00000
                        | encode(register_::_scratch) << 5);
    }

    //=== register operations ===//
    void mov(register_ rd, register_ r)
    {
        auto inst = 0b1'01'01010'00'0'00000'000000'11111'00000;
        inst |= encode(rd) << 0;
        inst |= encode(r) << 16;
        _inst.push_back(inst);
    }
    void mov_from_sp(register_ r)
    {
        auto inst = 0b1'0'0'100010'0'000000000000'11111'00000;
        inst |= encode(r) << 0;
        _inst.push_back(inst);
    }
    void mov_imm(register_ r, std::uint64_t imm)
    {
        if (imm <= UINT16_MAX)
        {
            // MOV r, #imm
            auto inst = 0b1'10'100101'00'0000000000000000'00000;
            inst |= imm << 5;
            inst |= encode(r) << 0;
            _inst.push_back(inst);
        }
        else
        {
            auto offset = literal(imm) & ((1 << 19) - 1);

            // LDR r, offset
            auto inst = 0b01'011'0'00 << 24;
            inst |= offset << 5;
            inst |= encode(r) << 0;
            _inst.push_back(inst);
        }
    }
    void mov_imm(register_ r, void* imm)
    {
        mov_imm(r, std::uint64_t(reinterpret_cast<std::uintptr_t>(imm)));
    }

    void adr(register_ rd, std::int32_t imm)
    {
        imm &= (1 << 21) - 1;

        auto inst = 0b0'00'10000 << 24;
        inst |= (imm & 0b11) << 29;
        inst |= (imm >> 2) << 5;
        inst |= encode(rd) << 0;
        _inst.push_back(inst);
    }

    void add_imm(register_ rd, register_ r, std::uint16_t imm)
    {
        // ADD rd, r, #imm
        auto inst = 0b1'0'0'100010'0'000000000000'00000'00000;
        inst |= encode(rd) << 0;
        inst |= encode(r) << 5;
        inst |= (imm & 0b111111111111) << 10;
        _inst.push_back(inst);
    }
    void sub_imm(register_ rd, register_ r, std::uint16_t imm)
    {
        // SUB rd, r, #imm
        auto inst = 0b1'1'0'100010'0'000000000000'00000'00000;
        inst |= encode(rd) << 0;
        inst |= encode(r) << 5;
        inst |= (imm & 0b111111111111) << 10;
        _inst.push_back(inst);
    }

    void cmp_imm(register_ r, std::uint16_t imm)
    {
        // CMP r, #imm
        auto inst = 0b1'1'1'100010'0'000000000000'00000'11111;
        inst |= encode(r) << 5;
        inst |= (imm & 0b111111111111) << 10;
        _inst.push_back(inst);
    }

    //=== memory ===//
    void push(register_ r)
    {
        // STR r, [SP, #-16]!
        // We subtract 16 as SP needs to have 16 byte alignment at all times.
        auto inst = 0b11'111'0'00'00'0'111110000'11'11111'00000;
        inst |= encode(r) << 0;
        _inst.push_back(inst);
    }
    void push_pair(register_ r1, register_ r2)
    {
        // STP r1, r2, [SP, #-16]!
        auto inst = 0b10'101'0'011'0'1111110'00000'11111'00000;
        inst |= encode(r2) << 10;
        inst |= encode(r1) << 0;
        _inst.push_back(inst);
    }

    void pop(register_ r)
    {
        // LDR r, [SP], #16
        // We add 16 as SP needs to have 16 byte alignment at all times.
        auto inst = 0b11'111'0'00'01'0'000010000'01'11111'00000;
        inst |= encode(r) << 0;
        _inst.push_back(inst);
    }
    void pop_pair(register_ r1, register_ r2)
    {
        // LDP r1, r2, [SP], #16
        auto inst = 0b10'101'0'001'1'0000010'00000'11111'00000;
        inst |= encode(r2) << 10;
        inst |= encode(r1) << 0;
        _inst.push_back(inst);
    }

    void str_imm(register_ reg, register_ base, std::ptrdiff_t index)
    {
        if (index >= 0)
        {
            // STR reg, base, #index
            auto inst = std::uint32_t(0b11'111'0'01'00) << 22;
            inst |= encode(reg) << 0;
            inst |= encode(base) << 5;
            inst |= (index & 0b111111111111) << 10;
            _inst.push_back(inst);
        }
        else if (index * 8 >= -256)
        {
            // STUR reg, base, #(index * 8)
            auto inst = std::uint32_t(0b11'111'0'00'00'0) << 21;
            inst |= encode(reg) << 0;
            inst |= encode(base) << 5;
            inst |= (std::uint32_t(index * 8) & 0b111111111) << 12;
            _inst.push_back(inst);
        }
        else
        {
            mov_imm(register_::_scratch, index);

            // STR reg, [base, scratch LSL 3]
            auto inst = 0b11'111'0'00'00'1'00000'0111'10'00000'00000;
            inst |= encode(reg) << 0;
            inst |= encode(base) << 5;
            inst |= encode(register_::_scratch) << 16;
            _inst.push_back(inst);
        }
    }
    void ldr_imm(register_ reg, register_ base, std::ptrdiff_t index)
    {
        if (index >= 0)
        {
            // LDR reg, base, #(index * 8)
            auto inst = std::uint32_t(0b11'111'0'01'01) << 22;
            inst |= encode(reg) << 0;
            inst |= encode(base) << 5;
            inst |= (index & 0b111111111111) << 10;
            _inst.push_back(inst);
        }
        else if (index * 8 >= -256)
        {
            // LDUR reg, base, #(index * 8)
            auto inst = std::uint32_t(0b11'111'0'00'01'0) << 21;
            inst |= encode(reg) << 0;
            inst |= encode(base) << 5;
            inst |= (std::uint32_t(index * 8) & 0b111111111) << 12;
            _inst.push_back(inst);
        }
        else
        {
            mov_imm(register_::_scratch, index);

            // LDR reg, [base, scratch LSL 3]
            auto inst = 0b11'111'0'00'01'1'00000'0111'10'00000'00000;
            inst |= encode(reg) << 0;
            inst |= encode(base) << 5;
            inst |= encode(register_::_scratch) << 16;
            _inst.push_back(inst);
        }
    }

    //=== finish ===//
    void reset()
    {
        _literals.reset();
        _inst.clear();
        _patches.clear();
        _labels.clear();
    }

    std::size_t instruction_count() const
    {
        return _inst.size();
    }

    std::size_t jit_size() const
    {
        return instruction_count() * sizeof(std::uint32_t) + _literals.size() * sizeof(lauf_value);
    }

    void* finish(void* memory)
    {
        for (auto p : _patches)
            resolve_patch(p);

        auto ptr = static_cast<unsigned char*>(memory);

        // Literals are stored in reverse.
        for (auto lit = _literals.data() + _literals.size(); lit != _literals.data(); --lit)
        {
            std::memcpy(ptr, lit - 1, sizeof(lauf_value));
            ptr += sizeof(lauf_value);
        }

        auto entry = ptr;
        std::memcpy(ptr, _inst.data(), _inst.size() * sizeof(std::uint32_t));
        return entry;
    }

    const std::uint32_t* data() const
    {
        return _inst.data();
    }

private:
    //=== literals ===//
    template <typename T>
    std::int32_t literal(T value)
    {
        auto lit_idx    = static_cast<std::ptrdiff_t>(_literals.insert(value));
        auto lit_offset = (lit_idx + 1) * sizeof(lauf_value);
        auto pc_offset  = _inst.size() * sizeof(std::uint32_t);
        return static_cast<std::int32_t>(-(lit_offset + pc_offset) / sizeof(std::uint32_t));
    }

    //=== patches ===//
    enum class branch_kind
    {
        unconditional,
        conditional,
    };

    std::uint32_t make_patch(std::size_t inst_idx, branch_kind kind)
    {
        return std::uint32_t(inst_idx << 1) | std::uint32_t(kind);
    }

    void resolve_patch(std::uint32_t patch)
    {
        auto idx  = patch >> 1;
        auto kind = branch_kind(patch & 0b1);

        auto [mask, shift] = [&] {
            switch (kind)
            {
            case branch_kind::unconditional:
                return std::make_pair((1 << 26) - 1, 0);
            case branch_kind::conditional:
                return std::make_pair(((1 << 19) - 1) << 5, 5);
            }
        }();

        auto lab    = (_inst[idx] & mask) >> shift;
        auto offset = std::int32_t(_labels[lab]) - std::int32_t(idx);

        _inst[idx] &= ~mask;
        _inst[idx] |= (offset << shift) & mask;
    }

    //=== data ===//
    literal_pool               _literals;
    std::vector<std::uint32_t> _inst;
    // Set of instruction indices that require patching.
    std::vector<std::uint32_t> _patches;
    // Mapping label index to position, -1 if unknown.
    std::vector<std::uint32_t> _labels;
};
} // namespace lauf::aarch64

#endif // SRC_LAUF_AARCH64_EMITTER_HPP_INCLUDED

