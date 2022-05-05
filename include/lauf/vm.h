// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_VM_H_INCLUDED
#define LAUF_VM_H_INCLUDED

#include <lauf/config.h>
#include <lauf/error.h>
#include <lauf/function.h>
#include <lauf/value.h>

LAUF_HEADER_START

typedef struct lauf_VMImpl* lauf_VM;

typedef struct lauf_VMOptions
{
    lauf_ErrorHandler error_handler;
    /// The maximal stack size in bytes.
    size_t max_stack_size;
} lauf_VMOptions;

extern const lauf_VMOptions lauf_default_vm_options;

lauf_VM lauf_vm(lauf_VMOptions options);

void lauf_vm_destroy(lauf_VM vm);

void lauf_vm_execute(lauf_VM vm, lauf_Function fn, const lauf_Value* input, lauf_Value* output);

LAUF_HEADER_END

#endif // LAUF_VM_H_INCLUDED

