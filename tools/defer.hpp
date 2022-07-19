// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef TOOLS_DEFER_HPP_INCLUDED
#define TOOLS_DEFER_HPP_INCLUDED

#include <optional>

namespace lauf
{
template <typename Fn>
struct deferer
{
    std::optional<Fn> _fn;

    explicit deferer(Fn fn) : _fn(std::move(fn)) {}

    deferer(deferer&& other) noexcept : _fn(std::move(other._fn))
    {
        other._fn.reset();
    }

    deferer& operator=(const deferer&) = delete;

    ~deferer()
    {
        if (_fn)
            (*_fn)();
    }
};

struct make_deferer
{
    template <typename Fn>
    friend auto operator+(make_deferer, Fn fn)
    {
        return deferer<Fn>(fn);
    }
};
} // namespace lauf

#define LAUF_CAT2(A, B) A##B
#define LAUF_CAT(A, B) LAUF_CAT2(A, B)

// NOLINTNEXTLINE
#define LAUF_DEFER const auto LAUF_CAT(lauf_deferer, __LINE__) = lauf::make_deferer{} + [&]

#define LAUF_DEFER_EXPR(...)                                                                       \
    LAUF_DEFER                                                                                     \
    {                                                                                              \
        __VA_ARGS__;                                                                               \
    }

#endif // TOOLS_DEFER_HPP_INCLUDED

