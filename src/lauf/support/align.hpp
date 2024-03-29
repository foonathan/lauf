// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_SUPPORT_ALIGN_HPP_INCLUDED
#define SRC_LAUF_SUPPORT_ALIGN_HPP_INCLUDED

#include <cassert>
#include <lauf/config.h>

namespace lauf
{
constexpr bool is_valid_alignment(std::size_t alignment) noexcept
{
    return alignment != 0u && (alignment & (alignment - 1)) == 0u;
}

constexpr std::uint8_t align_log2(std::size_t alignment) noexcept
{
    assert(is_valid_alignment(alignment));
    return std::uint8_t(__builtin_ctzll(alignment));
}

constexpr std::size_t align_offset(std::uintptr_t address, std::size_t alignment) noexcept
{
    assert(is_valid_alignment(alignment));
    auto misaligned = address & (alignment - 1);
    return misaligned != 0 ? (alignment - misaligned) : 0;
}
inline std::size_t align_offset(const void* address, std::size_t alignment) noexcept
{
    assert(is_valid_alignment(alignment));
    return align_offset(reinterpret_cast<std::uintptr_t>(address), alignment);
}

inline bool is_aligned(void* ptr, std::size_t alignment) noexcept
{
    return align_offset(ptr, alignment) == 0;
}

constexpr std::size_t round_to_multiple_of_alignment(std::size_t size,
                                                     std::size_t alignment) noexcept
{
    assert(is_valid_alignment(alignment));
    // The idea is as follows:
    // At worst, we need to add (alignment - 1) to align it properly.
    // We do that unconditionally and then strip away the last bits, so the number is a multiple.
    return (size + alignment - 1) & ~(alignment - 1);
}
} // namespace lauf

#endif // SRC_LAUF_SUPPORT_ALIGN_HPP_INCLUDED

