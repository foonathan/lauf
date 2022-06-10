// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_AARCH64_EMITTER_HPP_INCLUDED
#define SRC_LAUF_AARCH64_EMITTER_HPP_INCLUDED

#include <cstdint>
#include <cstring>
#include <lauf/config.h>
#include <vector>

namespace lauf::aarch64
{
enum class register_ : std::uint8_t
{
    scratch1 = 9,
    scratch2 = 10,

    // For emitter use only.
    _scratch = 17,
};

std::uint8_t encode(register_ reg)
{
    return std::uint8_t(reg) & 0b11111;
}

class emitter
{
public:
    //=== data ===//
    template <typename T>
    void data(T t)
    {
        static_assert(std::is_trivially_copyable_v<T> && sizeof(T) % sizeof(std::uint32_t) == 0);
        std::uint32_t data[sizeof(T) / sizeof(std::uint32_t)];
        std::memcpy(&data, &t, sizeof(T));
        for (auto i = 0u; i != sizeof(T) / sizeof(std::uint32_t); ++i)
            _inst.push_back(data[i]);
    }

    //=== control flow ===//
    void ret()
    {
        // RET X30
        _inst.push_back(0b1101011'0'0'10'11111'0000'0'0'11110'00000);
    }

    void b(std::int32_t offset)
    {
        // B #offset
        auto inst = 0b0'00101 << 26;
        inst |= offset & ((1 << 26) - 1);
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

        // LDR scratch, +2
        _inst.push_back(0b01'011'0'00'0000000000000000010'00000 | encode(register_::_scratch));
        // BR scratch
        _inst.push_back(0b1101011'0'0'00'11111'0000'0'0'00000'00000
                        | encode(register_::_scratch) << 5);
        // literal value
        data(fn);
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
        else if (imm <= UINT32_MAX)
        {
            // LDR r, +2 (32 bit)
            _inst.push_back(0b00'011'0'00'0000000000000000010'00000 | encode(r));
            // B +2
            b(2);
            // literal value
            data(std::uint32_t(imm));
        }
        else
        {
            // LDR r, +2 (64 bit)
            _inst.push_back(0b01'011'0'00'0000000000000000010'00000 | encode(r));
            // B +3
            b(3);
            // literal value
            data(imm);
        }
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

    void str_imm(register_ reg, register_ base, std::uint16_t index)
    {
        // STR reg, base, #(index * 8)
        auto inst = 0b11'111'0'01'00'000000000000'00000'00000;
        inst |= encode(reg) << 0;
        inst |= encode(base) << 5;
        inst |= (index & 0b111111111111) << 10;
        _inst.push_back(inst);
    }
    void ldr_imm(register_ reg, register_ base, std::uint16_t index)
    {
        // LDR reg, base, #(index * 8)
        auto inst = 0b11'111'0'01'01'000000000000'00000'00000;
        inst |= encode(reg) << 0;
        inst |= encode(base) << 5;
        inst |= (index & 0b111111111111) << 10;
        _inst.push_back(inst);
    }

    //=== finish ===//
    void clear()
    {
        _inst.clear();
    }

    std::size_t size() const
    {
        return _inst.size();
    }

    const std::uint32_t* data() const
    {
        return _inst.data();
    }

private:
    std::vector<std::uint32_t> _inst;
};
} // namespace lauf::aarch64

#endif // SRC_LAUF_AARCH64_EMITTER_HPP_INCLUDED

