// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_VM_JIT_HPP_INCLUDED
#define SRC_LAUF_VM_JIT_HPP_INCLUDED

#include <lauf/asm/module.h>

namespace lauf
{
bool jit_compile(lauf_asm_module* mod, lauf_asm_function* fn);
}

#endif // SRC_LAUF_VM_JIT_HPP_INCLUDED

