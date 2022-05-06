// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_VM_H_INCLUDED
#define LAUF_VM_H_INCLUDED

#include <lauf/config.h>
#include <lauf/error.h>
#include <lauf/module.h>
#include <lauf/value.h>

LAUF_HEADER_START

typedef struct lauf_vm_impl* lauf_vm;

typedef struct lauf_vm_options
{
    lauf_error_handler error_handler;
    /// The maximal stack size in bytes.
    size_t max_stack_size;
} lauf_vm_options;

extern const lauf_vm_options lauf_default_vm_options;

lauf_vm lauf_vm_create(lauf_vm_options options);

void lauf_vm_destroy(lauf_vm vm);

void lauf_vm_execute(lauf_vm vm, lauf_module mod, lauf_function fn, const lauf_value* input,
                     lauf_value* output);

LAUF_HEADER_END

#endif // LAUF_VM_H_INCLUDED

