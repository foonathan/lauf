// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_DETAIL_VERIFY_HPP_INCLUDED
#define SRC_DETAIL_VERIFY_HPP_INCLUDED

#include <cstdio>
#include <cstdlib>
#include <lauf/config.h>

#ifdef NDEBUG
#    define LAUF_DO_VERIFY 0
#else
#    define LAUF_DO_VERIFY 1
#endif

namespace lauf::_detail
{
inline void verification_failure(const char* inst, const char* msg)
{
    std::fprintf(stderr, "[lauf] %s: %s", inst, msg);
    std::abort();
}
} // namespace lauf::_detail

#if LAUF_DO_VERIFY
#    define LAUF_VERIFY(Cond, Inst, Msg)                                                           \
        ((Cond) ? void(0) : lauf::_detail::verification_failure(Inst, Msg))
#    define LAUF_VERIFY_RESULT(Cond, Inst, Msg) LAUF_VERIFY(Cond, Inst, Msg)
#else
#    define LAUF_VERIFY(Cond, Inst, Msg)
#    define LAUF_VERIFY_RESULT(Cond, Inst, Msg) Cond
#endif

#endif // SRC_DETAIL_VERIFY_HPP_INCLUDED

