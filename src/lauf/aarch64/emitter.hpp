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
class emitter
{
public:
    //=== control flow ===//
    void ret()
    {
        // RET X30
        _inst.push_back(0b1101011'0'0'10'11111'0000'0'0'11110'00000);
    }

    template <typename Fn>
    void tail_call(Fn* fn)
    {
        static_assert(std::is_function_v<Fn> && sizeof(fn) == sizeof(std::uint64_t));

        // LDR x9, +2
        _inst.push_back(0b01'011'0'00'0000000000000000010'01001);
        // BR x9
        _inst.push_back(0b1101011'0'0'00'11111'0000'0'0'01001'00000);
        // literal value
        std::uint32_t data[2];
        std::memcpy(&data, &fn, sizeof(fn));
        _inst.push_back(data[0]);
        _inst.push_back(data[1]);
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

