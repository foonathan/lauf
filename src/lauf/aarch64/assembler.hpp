// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_AARCH64_ASSEMBLER_HPP_INCLUDED
#define SRC_LAUF_AARCH64_ASSEMBLER_HPP_INCLUDED

#include <cstdint>
#include <lauf/config.h>
#include <lauf/support/temporary_array.hpp>
#include <lauf/support/verify.hpp>

namespace lauf::aarch64
{
template <std::size_t Width, typename T>
constexpr T _mask(const char* context, T value)
{
    constexpr auto bits = (T(1) << Width) - 1;

    auto result = value & bits;

    // We must either cut off only zero or one bits.
    auto other = (value & ~bits) >> Width;
    LAUF_VERIFY(other == 0 || other == ~bits >> Width, context, "encoding error");

    return std::make_unsigned_t<T>(value & bits);
}
} // namespace lauf::aarch64

namespace lauf::aarch64
{
//=== op ===//
template <std::size_t Shift = 0>
constexpr std::uint32_t encode(std::uint32_t op)
{
    return static_cast<std::uint32_t>(op) << Shift;
}

//=== register ===//
enum class register_nr : std::uint8_t
{
    frame = 29,
    link  = 30,
    stack = 31,
};

constexpr std::uint32_t encode(register_nr nr)
{
    return _mask<5>("aarch64 register", static_cast<std::uint32_t>(nr));
}

//=== immediate ===//
enum class immediate : std::int32_t
{
};

template <std::size_t Width>
constexpr std::uint32_t encode(immediate imm)
{
    return _mask<Width>("aarch64 immediate", static_cast<std::int32_t>(imm));
}

//=== condition code ===//
enum class condition_code : std::uint8_t
{
    eq,
    ne,
    cs,
    cc,
    mi,
    pl,
    vs,
    vc,
    hi,
    ls,
    ge,
    lt,
    gt,
    le,
    al,
    nv
};

constexpr std::uint32_t encode(condition_code cc)
{
    return static_cast<std::uint32_t>(cc);
}

//=== lsl ===//
enum class lsl : std::uint32_t
{
};

template <std::size_t Width, std::size_t Unit>
constexpr std::uint32_t encode(lsl l)
{
    auto value = static_cast<std::uint32_t>(l);
    LAUF_VERIFY(value % Unit == 0, "aarch64 LSL", "shift value not a valid multiple");
    value /= Unit;
    return _mask<Width>("aarch64 LSL", value);
}
} // namespace lauf::aarch64

namespace lauf::aarch64
{
struct code
{
    const std::uint32_t* ptr;
    std::size_t          size_in_bytes;
};

class assembler
{
public:
    explicit assembler(stack_allocator& alloc) : _alloc(&alloc), _insts(alloc, 64) {}

    void emit(std::uint32_t inst)
    {
        _insts.push_back(*_alloc, inst);
    }

    code finish()
    {
        for (auto p : _patches)
        {
            std::uint32_t mask  = 0;
            std::uint32_t shift = 0;
            switch (p.kind)
            {
            case patch::unconditional:
                mask  = (1 << 26) - 1;
                shift = 0;
                break;
            case patch::conditional:
                mask  = (1 << 19) - 1;
                shift = 5;
                break;
            }

            auto lab = (_insts[p.inst_idx] & mask) >> shift;
            assert(_label_pos[lab] != std::uint32_t(-1));
            auto offset = std::int32_t(_label_pos[lab]) - std::int32_t(p.inst_idx);

            _insts[p.inst_idx] &= ~(mask << shift);
            _insts[p.inst_idx] |= (offset & mask) << shift;
        }
        _patches.resize(0);

        return {_insts.data(), _insts.size() * sizeof(std::uint32_t)};
    }

    //=== label ===//
    enum class label : std::uint32_t
    {
    };

    label declare_label()
    {
        auto idx = _label_pos.size();
        _label_pos.push_back(*_alloc, std::uint32_t(-1));
        return label(idx);
    }

    void place_label(label l)
    {
        _label_pos[std::size_t(l)] = _insts.size();
    }

    //=== unconditional branch (register) ===//
private:
    void ub_reg_impl(std::uint32_t opc, std::uint32_t op3, std::uint32_t op4, register_nr rn)
    {
        auto inst = encode<25>(0b1101011) | encode<21>(opc) | encode<16>(0b11111) | encode<10>(op3)
                    | encode<0>(op4);
        inst |= encode(rn) << 5;
        emit(inst);
    }

public:
    void br(register_nr xn)
    {
        ub_reg_impl(0b0, 0b0, 0b0, xn);
    }
    void blr(register_nr xn)
    {
        ub_reg_impl(0b1, 0b0, 0b0, xn);
    }

    void ret(register_nr xn = register_nr::link)
    {
        ub_reg_impl(0b10, 0b0, 0b0, xn);
    }

    //=== unconditional branch (immediate) ===//
private:
    void ub_imm_impl(std::uint32_t op, immediate imm)
    {
        auto inst = encode<26>(0b00101) | encode<31>(op);
        inst |= encode<26>(imm);
        emit(inst);
    }

public:
    void b(immediate imm)
    {
        ub_imm_impl(0b0, imm);
    }
    void bl(immediate imm)
    {
        ub_imm_impl(0b1, imm);
    }

    void b(label l)
    {
        auto inst_idx = _insts.size();
        b(immediate(std::uint32_t(l)));
        _patches.push_back(*_alloc, {patch::unconditional, std::uint32_t(inst_idx)});
    }
    void bl(label l)
    {
        auto inst_idx = _insts.size();
        bl(immediate(std::uint32_t(l)));
        _patches.push_back(*_alloc, {patch::unconditional, std::uint32_t(inst_idx)});
    }

    //=== conditional branch (immediate) ===//
    void b(condition_code cc, immediate imm)
    {
        auto inst = encode<24>(0b01010100);
        inst |= encode(cc) << 0;
        inst |= encode<19>(imm) << 5;
        emit(inst);
    }

    void b(condition_code cc, label l)
    {
        auto inst_idx = _insts.size();
        b(cc, immediate(std::uint32_t(l)));
        _patches.push_back(*_alloc, {patch::conditional, std::uint32_t(inst_idx)});
    }

    //=== compare and branch ===//
private:
    void cb_impl(std::uint32_t op, register_nr xt, immediate imm)
    {
        auto inst = encode<25>(0b1'011010) | encode<24>(op);
        inst |= encode(xt) << 0;
        inst |= encode<19>(imm) << 5;
        emit(inst);
    }

public:
    void cbz(register_nr xt, immediate imm)
    {
        cb_impl(0b0, xt, imm);
    }
    void cbnz(register_nr xt, immediate imm)
    {
        cb_impl(0b1, xt, imm);
    }

    void cbz(register_nr xt, label l)
    {
        auto inst_idx = _insts.size();
        cbz(xt, immediate(std::uint32_t(l)));
        _patches.push_back(*_alloc, {patch::conditional, std::uint32_t(inst_idx)});
    }
    void cbnz(register_nr xt, label l)
    {
        auto inst_idx = _insts.size();
        cbnz(xt, immediate(std::uint32_t(l)));
        _patches.push_back(*_alloc, {patch::conditional, std::uint32_t(inst_idx)});
    }

    //=== load/store register (unsigned immediate) ===//
private:
    void ls_imm_impl(std::uint32_t size, std::uint32_t v, std::uint32_t opc, register_nr rt,
                     register_nr rn, immediate imm)
    {
        auto inst = encode<24>(0b00'111'0'01) | encode<30>(size) | encode<26>(v) | encode<22>(opc);
        inst |= encode(rt) << 0;
        inst |= encode(rn) << 5;
        inst |= encode<12>(imm) << 10;
        emit(inst);
    }

public:
    void str_imm(register_nr xt, register_nr xn, immediate imm)
    {
        ls_imm_impl(0b11, 0b0, 0b00, xt, xn, imm);
    }
    void ldr_imm(register_nr xt, register_nr xn, immediate imm)
    {
        ls_imm_impl(0b11, 0b0, 0b01, xt, xn, imm);
    }

    //=== load/store register (9 bit immediate) ===//
private:
    void ls_imm_impl(std::uint32_t size, std::uint32_t v, std::uint32_t opc, std::uint32_t op4,
                     register_nr rt, register_nr rn, immediate imm)
    {
        auto inst = encode<24>(0b00'111'0'00) | encode<30>(size) | encode<26>(v) | encode<22>(opc)
                    | encode<10>(op4);
        inst |= encode(rt) << 0;
        inst |= encode(rn) << 5;
        inst |= encode<9>(imm) << 12;
        emit(inst);
    }

public:
    void str_unscaled_imm(register_nr xt, register_nr xn, immediate imm)
    {
        ls_imm_impl(0b11, 0b0, 0b00, 0b00, xt, xn, imm);
    }
    void ldr_unscaled_imm(register_nr xt, register_nr xn, immediate imm)
    {
        ls_imm_impl(0b11, 0b0, 0b01, 0b00, xt, xn, imm);
    }

    void str_post_imm(register_nr xt, register_nr xn, immediate imm)
    {
        ls_imm_impl(0b11, 0b0, 0b00, 0b01, xt, xn, imm);
    }
    void ldr_post_imm(register_nr xt, register_nr xn, immediate imm)
    {
        ls_imm_impl(0b11, 0b0, 0b01, 0b01, xt, xn, imm);
    }

    void str_pre_imm(register_nr xt, register_nr xn, immediate imm)
    {
        ls_imm_impl(0b11, 0b0, 0b00, 0b11, xt, xn, imm);
    }
    void ldr_pre_imm(register_nr xt, register_nr xn, immediate imm)
    {
        ls_imm_impl(0b11, 0b0, 0b01, 0b11, xt, xn, imm);
    }

    //=== load/store register (register offset) ===//
private:
    void ls_impl(std::uint32_t opc, register_nr xt, register_nr xn, register_nr xm, lsl shift)
    {
        auto inst = encode<21>(0b11'111'0'00'001) | encode<22>(opc) | encode<13>(0b011)
                    | encode<10>(0b10);
        inst |= encode(xt) << 0;
        inst |= encode(xn) << 5;
        inst |= encode(xm) << 16;
        inst |= encode<1, 3>(shift) << 12;
        emit(inst);
    }

public:
    void str(register_nr xt, register_nr xn, register_nr xm, lsl shift = lsl{0})
    {
        ls_impl(0b00, xt, xn, xm, shift);
    }
    void ldr(register_nr xt, register_nr xn, register_nr xm, lsl shift = lsl{0})
    {
        ls_impl(0b01, xt, xn, xm, shift);
    }

    //=== load/store register pair ===//
private:
    void lsp_impl(std::uint32_t opc, std::uint32_t v, std::uint32_t l, std::uint32_t op2,
                  register_nr rt1, register_nr rt2, register_nr rn, immediate imm)
    {
        auto inst = encode<22>(0b00'101'0'000'0) | encode<30>(opc) | encode<26>(v) | encode<22>(l)
                    | encode<23>(op2);
        inst |= encode(rt1) << 0;
        inst |= encode(rt2) << 10;
        inst |= encode(rn) << 5;
        inst |= encode<7>(imm) << 15;
        emit(inst);
    }

public:
    void stp_post_imm(register_nr xt1, register_nr xt2, register_nr xn, immediate imm)
    {
        lsp_impl(0b10, 0b0, 0b0, 0b001, xt1, xt2, xn, imm);
    }
    void ldp_post_imm(register_nr xt1, register_nr xt2, register_nr xn, immediate imm)
    {
        lsp_impl(0b10, 0b0, 0b1, 0b001, xt1, xt2, xn, imm);
    }

    void stp(register_nr xt1, register_nr xt2, register_nr xn, immediate imm = immediate{0})
    {
        lsp_impl(0b10, 0b0, 0b0, 0b010, xt1, xt2, xn, imm);
    }
    void ldp(register_nr xt1, register_nr xt2, register_nr xn, immediate imm = immediate{0})
    {
        lsp_impl(0b10, 0b0, 0b1, 0b010, xt1, xt2, xn, imm);
    }

    void stp_pre_imm(register_nr xt1, register_nr xt2, register_nr xn, immediate imm)
    {
        lsp_impl(0b10, 0b0, 0b0, 0b011, xt1, xt2, xn, imm);
    }
    void ldp_pre_imm(register_nr xt1, register_nr xt2, register_nr xn, immediate imm)
    {
        lsp_impl(0b10, 0b0, 0b1, 0b011, xt1, xt2, xn, imm);
    }

    //=== arithmetic (immediate) ===//
private:
    void arithmetic_impl(std::uint32_t op_s, register_nr xd, register_nr xn, immediate imm,
                         lsl shift)
    {
        auto inst = encode<22>(0b1'00'100010'0) | encode<29>(op_s);
        inst |= encode(xd) << 0;
        inst |= encode(xn) << 5;
        inst |= encode<12>(imm) << 10;
        inst |= encode<1, 12>(shift) << 22;
        emit(inst);
    }

public:
    void add(register_nr xd, register_nr xn, immediate imm, lsl shift = lsl{0})
    {
        arithmetic_impl(0b00, xd, xn, imm, shift);
    }
    void adds(register_nr xd, register_nr xn, immediate imm, lsl shift = lsl{0})
    {
        arithmetic_impl(0b01, xd, xn, imm, shift);
    }
    void sub(register_nr xd, register_nr xn, immediate imm, lsl shift = lsl{0})
    {
        arithmetic_impl(0b10, xd, xn, imm, shift);
    }
    void subs(register_nr xd, register_nr xn, immediate imm, lsl shift = lsl{0})
    {
        arithmetic_impl(0b11, xd, xn, imm, shift);
    }
    void cmp(register_nr xd, register_nr xn, immediate imm, lsl shift = lsl{0})
    {
        arithmetic_impl(0b11, xd, xn, imm, shift);
    }
    void cmn(register_nr xd, register_nr xn, immediate imm, lsl shift = lsl{0})
    {
        arithmetic_impl(0b01, xd, xn, imm, shift);
    }

    //=== move (wide immediate) ===//
private:
    void mov_impl(std::uint32_t opc, register_nr xd, immediate imm, lsl shift)
    {
        auto inst = encode<23>(0b1'00'100101) | encode<29>(opc);
        inst |= encode(xd) << 0;
        inst |= encode<16>(imm) << 5;
        inst |= encode<2, 16>(shift) << 21;
        emit(inst);
    }

public:
    void movz(register_nr xd, immediate imm, lsl shift = lsl{0})
    {
        mov_impl(0b10, xd, imm, shift);
    }
    void movn(register_nr xd, immediate imm, lsl shift = lsl{0})
    {
        mov_impl(0b00, xd, imm, shift);
    }
    void movk(register_nr xd, immediate imm, lsl shift = lsl{0})
    {
        mov_impl(0b11, xd, imm, shift);
    }

    //=== mov (register) ===//
    void mov(register_nr xd, register_nr xm)
    {
        if (xd == register_nr::stack || xm == register_nr::stack)
        {
            // We don't want to use the zero register here.
            add(xd, xm, immediate{0});
        }
        else
        {
            // TODO: replace with orr once that's added.
            auto inst = encode<21>(0b1'01'01010'00'0) | encode<5>(0b11111);
            inst |= encode(xd) << 0;
            inst |= encode(xm) << 16;
            emit(inst);
        }
    }

    //=== impl ===//
private:
    struct patch
    {
        enum kind
        {
            unconditional,
            conditional,
        } kind : 1;
        std::uint32_t inst_idx : 31;
    };

    stack_allocator*               _alloc;
    temporary_array<std::uint32_t> _insts;
    temporary_array<patch>         _patches;
    temporary_array<std::uint32_t> _label_pos;
};
} // namespace lauf::aarch64

namespace lauf::aarch64
{
inline void stack_push(assembler& a, register_nr reg)
{
    // -2 because we need the stack alignment.
    a.str_pre_imm(reg, register_nr::stack, immediate(-2));
}
inline void stack_push(assembler& a, register_nr reg1, register_nr reg2)
{
    a.stp_pre_imm(reg1, reg2, register_nr::stack, immediate(-2));
}

inline void stack_allocate(assembler& a, std::uint16_t size)
{
    if (size == 0)
        return;

    if (auto remainder = size % 16)
        size += 16 - remainder;
    a.sub(register_nr::stack, register_nr::stack, immediate(size));
}

inline void stack_free(assembler& a, std::uint16_t size)
{
    if (size == 0)
        return;

    if (auto remainder = size % 16)
        size += 16 - remainder;
    a.add(register_nr::stack, register_nr::stack, immediate(size));
}

inline void stack_pop(assembler& a, register_nr reg)
{
    // 2 because we need the stack alignment.
    a.ldr_post_imm(reg, register_nr::stack, immediate(2));
}
inline void stack_pop(assembler& a, register_nr reg1, register_nr reg2)
{
    a.ldp_post_imm(reg1, reg2, register_nr::stack, immediate(2));
}
} // namespace lauf::aarch64

#endif // SRC_LAUF_AARCH64_ASSEMBLER_HPP_INCLUDED

