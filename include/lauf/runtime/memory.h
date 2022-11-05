// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_RUNTIME_MEMORY_H_INCLUDED
#define LAUF_RUNTIME_MEMORY_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_asm_function             lauf_asm_function;
typedef struct lauf_asm_global               lauf_asm_global;
typedef struct lauf_asm_layout               lauf_asm_layout;
typedef struct lauf_asm_signature            lauf_asm_signature;
typedef struct lauf_runtime_address          lauf_runtime_address;
typedef struct lauf_runtime_fiber            lauf_runtime_fiber;
typedef struct lauf_runtime_function_address lauf_runtime_function_address;
typedef struct lauf_runtime_process          lauf_runtime_process;
typedef union lauf_runtime_value             lauf_runtime_value;

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

/// Returns the address of a global variable of the process.
lauf_runtime_address lauf_runtime_get_global_address(lauf_runtime_process*  p,
                                                     const lauf_asm_global* global);

/// Converts a function address into a function pointer if it is valid.
/// Returns null otherwise.
const lauf_asm_function* lauf_runtime_get_function_ptr_any(lauf_runtime_process*         p,
                                                           lauf_runtime_function_address addr);

/// Converts a function address into a function pointer if it is valid and has the specified
/// signature. Returns null otherwise.
const lauf_asm_function* lauf_runtime_get_function_ptr(lauf_runtime_process*         p,
                                                       lauf_runtime_function_address addr,
                                                       lauf_asm_signature            signature);

/// Converts an address into a fiber if it is a valid handle.
/// Returns nulll otherwise.
lauf_runtime_fiber* lauf_runtime_get_fiber_ptr(lauf_runtime_process* p, lauf_runtime_address addr);

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

/// Adds a new static immutable allocation and returns its address.
/// Bytecode can read it.
lauf_runtime_address lauf_runtime_add_static_const_allocation(lauf_runtime_process* p,
                                                              const void* ptr, size_t size);

/// Adds a new static immutable allocation and returns its address.
/// Bytecode can read and write it.
lauf_runtime_address lauf_runtime_add_static_mut_allocation(lauf_runtime_process* p, void* ptr,
                                                            size_t size);

/// Adds a new heap allocation and returns its address.
/// Bytecode can read, write, and free this allocation using the allocator of the VM.
lauf_runtime_address lauf_runtime_add_heap_allocation(lauf_runtime_process* p, void* ptr,
                                                      size_t size);

/// Leaks a heap allocation.
/// It is marked as freed, but not actually freed.
bool lauf_runtime_leak_heap_allocation(lauf_runtime_process* p, lauf_runtime_address addr);

/// Frees all heap allocated memory and fibers that are not reachable.
///
/// It uses a conservative tracing algorithm that assumes anything that could be a valid address is
/// one. Addresses with invalid offsets do not keep the allocation alive.
///
/// Returns the total number of bytes freed.
size_t lauf_runtime_gc(lauf_runtime_process* p);

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

#endif // LAUF_RUNTIME_MEMORY_H_INCLUDED

