// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_VM_H_INCLUDED
#define LAUF_VM_H_INCLUDED

#include <lauf/config.h>
#include <lauf/program.h>
#include <lauf/value.h>

LAUF_HEADER_START

typedef struct lauf_vm_impl* lauf_vm;

//=== panic_handler ===//
typedef struct lauf_panic_info_impl* lauf_panic_info;

typedef void (*lauf_panic_handler)(lauf_panic_info info, const char* message);

//=== options ===//
typedef struct lauf_vm_options
{
    size_t             max_value_stack_size;
    lauf_panic_handler panic_handler;
} lauf_vm_options;

extern const lauf_vm_options lauf_default_vm_options;

//=== functions ===//
lauf_vm lauf_vm_create(lauf_vm_options options);

void lauf_vm_destroy(lauf_vm vm);

/// Executes the given program, reading the input values from `input` and writing the output values
/// to `output`. On success, returns true. On error, returns false after invoking the panic handler.
bool lauf_vm_execute(lauf_vm vm, lauf_program prog, const lauf_value* input, lauf_value* output);

LAUF_HEADER_END

#endif // LAUF_VM_H_INCLUDED

