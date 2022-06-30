// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/jit.h>

lauf_jit_compiler lauf_jit_compiler_create(void)
{
    return nullptr;
}

void lauf_jit_compiler_destroy(lauf_jit_compiler) {}

lauf_jit_compiler lauf_vm_jit_compiler(lauf_vm)
{
    return nullptr;
}

bool lauf_jit_compile(lauf_jit_compiler, lauf_function)
{
    return false;
}

