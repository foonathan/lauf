// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_RUNTIME_PROCESS_H_INCLUDED
#define LAUF_RUNTIME_PROCESS_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_asm_function       lauf_asm_function;
typedef struct lauf_asm_program        lauf_asm_program;
typedef struct lauf_runtime_stacktrace lauf_runtime_stacktrace;
typedef struct lauf_vm                 lauf_vm;
typedef union lauf_asm_inst            lauf_asm_inst;
typedef union lauf_runtime_value       lauf_runtime_value;

/// Represents a running lauf program.
typedef struct lauf_runtime_process lauf_runtime_process;

/// Represents a fiber in a process.
typedef struct lauf_runtime_fiber lauf_runtime_fiber;

//=== queries ===//
/// The VM that is executing the program.
lauf_vm* lauf_runtime_get_vm(lauf_runtime_process* p);

/// The program that is running.
const lauf_asm_program* lauf_runtime_get_program(lauf_runtime_process* p);

/// The currently active fiber.
const lauf_runtime_fiber* lauf_runtime_get_current_fiber(lauf_runtime_process* p);

/// Iterates over the fibers in arbitrary order.
const lauf_runtime_fiber* lauf_runtime_iterate_fibers(lauf_runtime_process* p);
const lauf_runtime_fiber* lauf_runtime_iterate_fibers_next(const lauf_runtime_fiber* iter);

//=== fiber queries ===//
/// Returns the base of the vstack (highest address as it grows down) of the fiber.
const lauf_runtime_value* lauf_runtime_get_vstack_base(const lauf_runtime_fiber* fiber);

//=== actions ===//
/// Calls the given function on a fresh fiber.
///
/// It behaves like `lauf_vm_execute()` but re-uses the existing VM of the process.
/// The function must be part of the program.
bool lauf_runtime_call(lauf_runtime_process* p, const lauf_asm_function* fn,
                       const lauf_runtime_value* input, lauf_runtime_value* output);

/// Triggers a panic.
///
/// It invokes the panic handler, but does not abort or anything.
/// The builtin needs to do that by returning false.
///
/// The function always returns false for convenience.
bool lauf_runtime_panic(lauf_runtime_process* p, const char* msg);

/// Limits the number of execution steps that can be taken before the process panics.
///
/// This has no effect, unless `lauf_lib_limitis_step[s]` is called, which modifies the limit.
/// The limit cannot be increased beyond the limit provided in the VM config.
/// It also reaches the existing steps taken.
bool lauf_runtime_set_step_limit(lauf_runtime_process* p, size_t new_limit);

/// Increments the step count by one.
///
/// If this reaches the step limit, returns false.
/// It needs to be manually inserted during codegen (e.g. once per function and loop iteratation)
/// for the step limit to work. If the step limit is unlimited, does nothing.
bool lauf_runtime_increment_step(lauf_runtime_process* p);

LAUF_HEADER_END

#endif // LAUF_RUNTIME_PROCESS_H_INCLUDED

