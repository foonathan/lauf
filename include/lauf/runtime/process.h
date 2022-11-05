// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_RUNTIME_PROCESS_H_INCLUDED
#define LAUF_RUNTIME_PROCESS_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_asm_function       lauf_asm_function;
typedef struct lauf_asm_program        lauf_asm_program;
typedef struct lauf_runtime_stacktrace lauf_runtime_stacktrace;
typedef struct lauf_runtime_address    lauf_runtime_address;
typedef struct lauf_vm                 lauf_vm;
typedef union lauf_asm_inst            lauf_asm_inst;
typedef union lauf_runtime_value       lauf_runtime_value;

/// Represents a running lauf program.
typedef struct lauf_runtime_process lauf_runtime_process;

/// Represents a fiber in a process.
typedef struct lauf_runtime_fiber lauf_runtime_fiber;

/// The status of a fiber.
typedef enum lauf_runtime_fiber_status
{
    /// The fiber has been created but not yet started.
    LAUF_RUNTIME_FIBER_READY,
    /// The fiber is currently running.
    LAUF_RUNTIME_FIBER_RUNNING,
    /// The fiber is currently suspended.
    LAUF_RUNTIME_FIBER_SUSPENDED,
    /// The fiber is done.
    LAUF_RUNTIME_FIBER_DONE,
} lauf_runtime_fiber_status;

//=== queries ===//
/// The VM that is executing the program.
lauf_vm* lauf_runtime_get_vm(lauf_runtime_process* process);

/// The program that is running.
const lauf_asm_program* lauf_runtime_get_program(lauf_runtime_process* process);

/// The currently active fiber.
/// Returns the fiber that was last active if all fibers are currently suspended.
lauf_runtime_fiber* lauf_runtime_get_current_fiber(lauf_runtime_process* process);

/// Iterates over the fibers in arbitrary order.
lauf_runtime_fiber* lauf_runtime_iterate_fibers(lauf_runtime_process* process);
lauf_runtime_fiber* lauf_runtime_iterate_fibers_next(lauf_runtime_fiber* iter);

/// Returns whether there is only a single fiber at the moment.
bool lauf_runtime_is_single_fibered(lauf_runtime_process* process);

//=== fiber queries ===//
/// Returns a handle to the specified fiber.
lauf_runtime_address lauf_runtime_get_fiber_handle(const lauf_runtime_fiber* fiber);

/// Returns the status of the fiber.
lauf_runtime_fiber_status lauf_runtime_get_fiber_status(const lauf_runtime_fiber* fiber);

/// Returns the parent of the fiber, if it has any.
lauf_runtime_fiber* lauf_runtime_get_fiber_parent(lauf_runtime_process* process,
                                                  lauf_runtime_fiber*   fiber);

/// Returns the current position of the vstack (lowest address as it grows down) of the fiber.
const lauf_runtime_value* lauf_runtime_get_vstack_ptr(lauf_runtime_process*     process,
                                                      const lauf_runtime_fiber* fiber);

/// Returns the base of the vstack (highest address as it grows down) of the fiber.
const lauf_runtime_value* lauf_runtime_get_vstack_base(const lauf_runtime_fiber* fiber);

//=== actions ===//
/// Calls the given function on a fresh fiber and keeps resuming it until it finishes.
///
/// It behaves like `lauf_vm_execute()` but re-uses the existing VM of the process.
/// The function must be part of the program.
bool lauf_runtime_call(lauf_runtime_process* process, const lauf_asm_function* fn,
                       const lauf_runtime_value* input, lauf_runtime_value* output);

/// Creates a new fiber.
///
/// It will start executing the specified function, which must be part of the process, once resumed.
lauf_runtime_fiber* lauf_runtime_create_fiber(lauf_runtime_process*    process,
                                              const lauf_asm_function* fn);

/// Resumes the specified fiber.
///
/// This activates the fiber and executes it until it suspends again.
/// It returns false if it panics, true otherwise.
/// The input values are passed to the fiber.
///
/// It must only be called when no fiber is currently running.
bool lauf_runtime_resume(lauf_runtime_process* process, lauf_runtime_fiber* fiber,
                         const lauf_runtime_value* input, size_t input_count);

/// Destroys a fiber.
///
/// If the fiber is not yet done, it will cancel it.
void lauf_runtime_destroy_fiber(lauf_runtime_process* process, lauf_runtime_fiber* fiber);

/// Triggers a panic.
///
/// It invokes the panic handler, but does not abort or anything.
/// The builtin needs to do that by returning false.
///
/// The function always returns false for convenience.
bool lauf_runtime_panic(lauf_runtime_process* process, const char* msg);

/// Destroys the process and releases all memory associated with it.
/// The VM is free to be re-used to start another process.
void lauf_runtime_destroy_process(lauf_runtime_process* process);

//=== steps ===//
/// Limits the number of execution steps that can be taken before the process panics.
///
/// This has no effect, unless `lauf_lib_limitis_step[s]` is called, which modifies the limit.
/// The limit cannot be increased beyond the limit provided in the VM config.
/// It also reaches the existing steps taken.
bool lauf_runtime_set_step_limit(lauf_runtime_process* process, size_t new_limit);

/// Increments the step count by one.
///
/// If this reaches the step limit, returns false.
/// It needs to be manually inserted during codegen (e.g. once per function and loop iteratation)
/// for the step limit to work. If the step limit is unlimited, does nothing.
bool lauf_runtime_increment_step(lauf_runtime_process* process);

LAUF_HEADER_END

#endif // LAUF_RUNTIME_PROCESS_H_INCLUDED

