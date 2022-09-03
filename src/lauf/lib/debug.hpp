// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_LIB_DEBUG_HPP_INCLUDED
#define SRC_LAUF_LIB_DEBUG_HPP_INCLUDED

#include <lauf/lib/debug.h>

#include <lauf/runtime/process.h>
#include <lauf/runtime/value.h>

namespace lauf
{
void debug_print(lauf_runtime_process* process, lauf_runtime_value value);
void debug_print_cstack(lauf_runtime_process* process, const lauf_runtime_fiber* fiber);
void debug_print_all_cstacks(lauf_runtime_process* process);
} // namespace lauf

#endif // SRC_LAUF_LIB_DEBUG_HPP_INCLUDED

