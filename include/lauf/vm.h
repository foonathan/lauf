// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_VM_H_INCLUDED
#define LAUF_VM_H_INCLUDED

#include <lauf/config.h>
#include <lauf/program.h>
#include <lauf/value.h>

LAUF_HEADER_START

typedef struct lauf_vm_impl*         lauf_vm;
typedef struct lauf_vm_process_impl* lauf_vm_process;
typedef union lauf_vm_instruction    lauf_vm_instruction;

//=== backtrace ===//
typedef void* lauf_backtrace;

lauf_function       lauf_backtrace_get_function(lauf_backtrace bt);
lauf_debug_location lauf_backtrace_get_location(lauf_backtrace bt);
lauf_backtrace      lauf_backtrace_parent(lauf_backtrace bt);

//=== panic_handler ===//
typedef struct lauf_panic_info_impl* lauf_panic_info;

lauf_backtrace lauf_panic_info_get_backtrace(lauf_panic_info info);

typedef void (*lauf_panic_handler)(lauf_panic_info info, const char* message);

//=== allocation ===//
typedef struct lauf_vm_allocator
{
    void* user_data;
    void* (*heap_alloc)(void* user_data, size_t size, size_t alignment);
    void (*free_alloc)(void* user_data, void* ptr);
} lauf_vm_allocator;

extern const lauf_vm_allocator lauf_vm_null_allocator;
extern const lauf_vm_allocator lauf_vm_malloc_allocator;

//=== options ===//
typedef struct lauf_vm_options
{
    size_t             max_value_stack_size, max_stack_size;
    lauf_panic_handler panic_handler;
    lauf_vm_allocator  allocator;
} lauf_vm_options;

extern const lauf_vm_options lauf_default_vm_options;

//=== functions ===//
lauf_vm lauf_vm_create(lauf_vm_options options);

void lauf_vm_destroy(lauf_vm vm);

void lauf_vm_set_panic_handler(lauf_vm vm, lauf_panic_handler handler);

/// Executes the given program, reading the input values from `input` and writing the output values
/// to `output`. On success, returns true. On error, returns false after invoking the panic handler.
bool lauf_vm_execute(lauf_vm vm, lauf_program prog, const lauf_value* input, lauf_value* output);

LAUF_HEADER_END

#endif // LAUF_VM_H_INCLUDED

