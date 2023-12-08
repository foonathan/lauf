// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_VM_H_INCLUDED
#define LAUF_VM_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_asm_program     lauf_asm_program;
typedef struct lauf_runtime_process lauf_runtime_process;
typedef union lauf_runtime_value    lauf_runtime_value;

//=== vm options ===//
typedef struct lauf_vm_panic_handler
{
    void* user_data;
    void (*callback)(void* user_data, lauf_runtime_process* p, const char* msg);
} lauf_vm_panic_handler;

typedef struct lauf_vm_allocator
{
    void* user_data;
    void* (*heap_alloc)(void* user_data, size_t size, size_t alignment);
    // size may be 0 if it is not known.
    void (*free_alloc)(void* user_data, void* ptr, size_t size);
} lauf_vm_allocator;

extern const lauf_vm_allocator lauf_vm_null_allocator;
extern const lauf_vm_allocator lauf_vm_malloc_allocator;

typedef struct lauf_vm_options
{
    /// The initial size of the value stack in elements.
    size_t initial_vstack_size_in_elements;
    /// The maximum size of the value stack in elements.
    size_t max_vstack_size_in_elements;

    /// The maximum size of the call stack.
    size_t max_cstack_size_in_bytes;
    /// The initial size of the call stack, it can grow bigger if necessary.
    size_t initial_cstack_size_in_bytes;

    /// The initial max step value (see lauf_lib_limits_set_step_limit).
    /// A value of zero means unlimited.
    size_t step_limit;

    /// A handler that is called when a process panics.
    lauf_vm_panic_handler panic_handler;
    /// The allocator used when the program wants to allocate heap memory.
    lauf_vm_allocator allocator;

    // Arbitrary user data.
    void* user_data;
} lauf_vm_options;

extern const lauf_vm_options lauf_default_vm_options;

//=== vm ===//
typedef struct lauf_vm lauf_vm;

lauf_vm* lauf_create_vm(lauf_vm_options options);
void     lauf_destroy_vm(lauf_vm* vm);

/// Exchanges the panic handler.
lauf_vm_panic_handler lauf_vm_set_panic_handler(lauf_vm* vm, lauf_vm_panic_handler h);

/// Exchanges the allocator.
lauf_vm_allocator lauf_vm_set_allocator(lauf_vm* vm, lauf_vm_allocator a);
/// Returns the allocator.
lauf_vm_allocator lauf_vm_get_allocator(lauf_vm* vm);

/// Exchanges the user data.
void* lauf_vm_set_user_data(lauf_vm* vm, void* user_data);
/// Returns the user data.
void* lauf_vm_get_user_data(lauf_vm* vm);

/// Starts a new process for the program.
///
/// It creates a fiber for the entry function and turns it into the current fiber,
/// but does not start running it yet; use `lauf_runtime_resume()` for that.
lauf_runtime_process* lauf_vm_start_process(lauf_vm* vm, const lauf_asm_program* program);

/// Executes the program on the VM.
///
/// `input` is an array that contains as many values as specified by the input signature of the
/// entry function. Those values will become the arguments to the function: `input[0]` is the first
/// argument on the bottom, `input[N]` the last argument on the top.
///
/// `output` is an array that has space for as many values as specified by the output signature of
/// the entry function. Those values will be copied from the bottom of the value stack: `output[0]`
/// is the first output value on the stack bottom, `output[N]` the last output value on the top.
///
/// If a fiber suspends, it will repeatedly resume it until the main fiber finishes.
/// It returns `true` if execution finished without panicing, `false` otherwise.
/// If it returns `false`, `output` has not been modified.
bool lauf_vm_execute(lauf_vm* vm, const lauf_asm_program* program, //
                     const lauf_runtime_value* input, lauf_runtime_value* output);

/// Executes the program on the VM and destroys it afterwards.
///
/// It behaves the same as `lauf_vm_execute()` followed by `lauf_asm_program_destroy()`.
bool lauf_vm_execute_oneshot(lauf_vm* vm, lauf_asm_program program, //
                             const lauf_runtime_value* input, lauf_runtime_value* output);

LAUF_HEADER_END

#endif // LAUF_VM_H_INCLUDED

