// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_RUNTIME_PROCESS_H_INCLUDED
#define LAUF_RUNTIME_PROCESS_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_asm_layout               lauf_asm_layout;
typedef struct lauf_asm_program              lauf_asm_program;
typedef struct lauf_asm_function             lauf_asm_function;
typedef union lauf_asm_inst                  lauf_asm_inst;
typedef struct lauf_asm_signature            lauf_asm_signature;
typedef struct lauf_runtime_stacktrace       lauf_runtime_stacktrace;
typedef struct lauf_runtime_address          lauf_runtime_address;
typedef struct lauf_runtime_function_address lauf_runtime_function_address;
typedef union lauf_runtime_value             lauf_runtime_value;
typedef struct lauf_vm                       lauf_vm;

/// Represents a currently running lauf program.
typedef struct lauf_runtime_process lauf_runtime_process;

/// A stack frame of the process.
typedef struct lauf_runtime_stack_frame lauf_runtime_stack_frame;

//=== queries ===//
/// The VM that is executing the program.
lauf_vm* lauf_runtime_get_vm(lauf_runtime_process* p);

/// The program that is running.
const lauf_asm_program* lauf_runtime_get_program(lauf_runtime_process* p);

/// Returns the base of the vstack (highest address as it grows down).
const lauf_runtime_value* lauf_runtime_get_vstack_base(lauf_runtime_process* p);

/// Returns the current stacktrace of the process.
lauf_runtime_stacktrace* lauf_runtime_get_stacktrace(lauf_runtime_process* p);

//=== actions ===//
/// Calls the given function.
///
/// It behaves like `lauf_vm_execute()` but re-uses the existing VM of the process.
/// The function must be part of the program.
bool lauf_runtime_call(lauf_runtime_process* p, const lauf_asm_function* fn,
                       lauf_runtime_value* vstack_ptr);

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

//=== address ===//
/// Converts an address to a pointer if the address is readable for the layout.
/// Returns null otherwise.
const void* lauf_runtime_get_const_ptr(lauf_runtime_process* p, lauf_runtime_address addr,
                                       lauf_asm_layout layout);

/// Converts an address to a pointer if the address is readable and writeable for the layout.
/// Returns null otherwise.
void* lauf_runtime_get_mut_ptr(lauf_runtime_process* p, lauf_runtime_address addr,
                               lauf_asm_layout layout);

/// Converts an address to a pointer if there is a null-terminated string starting at address.
/// Returns null otherwise.
const char* lauf_runtime_get_cstr(lauf_runtime_process* p, lauf_runtime_address addr);

/// Gets the address for a native pointer of the specified allocation.
/// On success, updates `allocation` to the actual address.
bool lauf_runtime_get_address(lauf_runtime_process* p, lauf_runtime_address* allocation,
                              const void* ptr);

/// Converts a function address into a function pointer if it is valid.
/// Returns null otherwise.
const lauf_asm_function* lauf_runtime_get_function_ptr_any(lauf_runtime_process*         p,
                                                           lauf_runtime_function_address addr);

/// Converts a function address into a function pointer if it is valid and has the specified
/// signature. Returns null otherwise.
const lauf_asm_function* lauf_runtime_get_function_ptr(lauf_runtime_process*         p,
                                                       lauf_runtime_function_address addr,
                                                       lauf_asm_signature            signature);

//=== allocations ===//
typedef enum lauf_runtime_allocation_source
{
    LAUF_RUNTIME_STATIC_ALLOCATION,
    LAUF_RUNTIME_LOCAL_ALLOCATION,
    LAUF_RUNTIME_HEAP_ALLOCATION,
} lauf_runtime_allocation_source;

typedef enum lauf_runtime_permission
{
    LAUF_RUNTIME_PERM_NONE       = 0,
    LAUF_RUNTIME_PERM_READ       = 1 << 0,
    LAUF_RUNTIME_PERM_WRITE      = 1 << 1,
    LAUF_RUNTIME_PERM_READ_WRITE = LAUF_RUNTIME_PERM_READ | LAUF_RUNTIME_PERM_WRITE,
} lauf_runtime_permission;

typedef struct lauf_runtime_allocation
{
    lauf_runtime_allocation_source source;
    lauf_runtime_permission        permission;
    void*                          ptr;
    size_t                         size;
} lauf_runtime_allocation;

/// Returns metadata of an allocation.
bool lauf_runtime_get_allocation(lauf_runtime_process* p, lauf_runtime_address addr,
                                 lauf_runtime_allocation* result);

/// Adds a new heap allocation and returns its address.
lauf_runtime_address lauf_runtime_add_heap_allocation(lauf_runtime_process* p, void* ptr,
                                                      size_t size);

/// Leaks a heap allocation.
/// It is marked as freed, but not actually freed.
bool lauf_runtime_leak_heap_allocation(lauf_runtime_process* p, lauf_runtime_address addr);

/// Frees all heap allocated memory that is not reachable.
///
/// It uses a conservative tracing algorithm that assumes anything that could be a valid address is
/// one. Addresses with invalid offsets do not keep the allocation alive.
///
/// Returns the total number of bytes freed.
size_t lauf_runtime_gc(lauf_runtime_process* p, const lauf_runtime_value* vstack_ptr);

/// Poisons the allocation an address is in.
///
/// It may not be accessed until un-poisoned again, but can be freed.
/// Poisoning a local variable may not have any effect if its pointer arithmetic has been optimized
/// out.
bool lauf_runtime_poison_allocation(lauf_runtime_process* p, lauf_runtime_address addr);

/// Unpoisons an allocation that was previously poisoned.
bool lauf_runtime_unpoison_allocation(lauf_runtime_process* p, lauf_runtime_address addr);

/// Splits an allocation into two parts.
///
/// Let `addr` be an address for allocation `a` at offset `o`, which must not be the end.
/// Calling split shrinks the allocation `a` to size `o`,
/// and creates a new allocation `a'` for the region beginning at `o` until the original end of `a`.
/// It sets `addr1` to `a` at offset `0` and a `addr2` to the beginning of `a'`.
///
/// Existing addresses for `a` are only valid if their offset is `< o`; otherwise, they become
/// invalidated. Neither `a` nor `a'` can be freed until the allocation has been merged again.
bool lauf_runtime_split_allocation(lauf_runtime_process* p, lauf_runtime_address addr,
                                   lauf_runtime_address* addr1, lauf_runtime_address* addr2);

/// Merges an allocation that has been previously split.
///
/// It requires that `addr1` and `addr2` come from a call to `lauf_lib_memory_split` (ignoring the
/// offsets).
///
/// All pointers to the allocation of `addr2` are invalidated, pointers to allocation of `addr1`
/// remain valid, and any pointers created before the split become valid again.
bool lauf_runtime_merge_allocation(lauf_runtime_process* p, lauf_runtime_address addr1,
                                   lauf_runtime_address addr2);

/// Marks a heap allocation as reachable for the purposes of garbage collection.
bool lauf_runtime_declare_reachable(lauf_runtime_process* p, lauf_runtime_address addr);

/// Unmarks a heap allocation as reachable for the purposes of garbage collection.
bool lauf_runtime_undeclare_reachable(lauf_runtime_process* p, lauf_runtime_address addr);

/// Marks an (arbitrary) allocation as weak for the purposes of garbage collection.
///
/// When determening whether an allocation is reachable, any addresses inside weak allocations are
/// not considered.
bool lauf_runtime_declare_weak(lauf_runtime_process* p, lauf_runtime_address addr);

/// Undeclares an allocation as weak.
bool lauf_runtime_undeclare_weak(lauf_runtime_process* p, lauf_runtime_address addr);

LAUF_HEADER_END

#endif // LAUF_RUNTIME_PROCESS_H_INCLUDED

