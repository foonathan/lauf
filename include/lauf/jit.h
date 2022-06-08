// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_JIT_H_INCLUDED
#define LAUF_JIT_H_INCLUDED

#include <lauf/builtin.h>
#include <lauf/config.h>
#include <lauf/module.h>

LAUF_HEADER_START

typedef struct lauf_jit_compiler_impl* lauf_jit_compiler;

/// Creates a JIT compiler.
lauf_jit_compiler lauf_jit_compiler_create(void);
/// Destroys the JIT compiler; does not affect any existing function pointers.
void lauf_jit_compiler_destroy(lauf_jit_compiler compiler);

/// Returns the JIT compiler used by the VM.
lauf_jit_compiler lauf_vm_jit_compiler(lauf_vm vm);

/// JIT compiles the given function, or returns a cached version of it.
/// Returns NULL, if JIT not available.
lauf_builtin_function* lauf_jit_compile(lauf_jit_compiler compiler, lauf_function fn);

LAUF_HEADER_END

#endif // LAUF_JIT_H_INCLUDED

