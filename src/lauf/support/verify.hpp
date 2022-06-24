// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_SUPPORT_VERIFY_HPP_INCLUDED
#define SRC_LAUF_SUPPORT_VERIFY_HPP_INCLUDED

#include <cstdio>
#include <cstdlib>
#include <lauf/config.h>

#ifdef NDEBUG
#    define LAUF_DO_VERIFY 0
#else
#    define LAUF_DO_VERIFY 1
#endif

namespace lauf
{
inline void verification_failure(const char* context, const char* msg)
{
    std::fprintf(stderr, "[lauf] %s: %s\n", context, msg);
    std::abort();
}
} // namespace lauf

#if LAUF_DO_VERIFY
#    define LAUF_VERIFY(Cond, Context, Msg)                                                        \
        ((Cond) ? void(0) : lauf::verification_failure(Context, Msg))
#    define LAUF_VERIFY_RESULT(Cond, Context, Msg) LAUF_VERIFY(Cond, Context, Msg)
#else
#    define LAUF_VERIFY(Cond, Context, Msg)
#    define LAUF_VERIFY_RESULT(Cond, Context, Msg) Cond
#endif

#endif // SRC_LAUF_SUPPORT_VERIFY_HPP_INCLUDED

