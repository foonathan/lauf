// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_ASM_TYPE_H_INCLUDED
#define LAUF_ASM_TYPE_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef union lauf_asm_inst             lauf_asm_inst;
typedef union lauf_runtime_value        lauf_runtime_value;
typedef struct lauf_runtime_process     lauf_runtime_process;
typedef struct lauf_runtime_stack_frame lauf_runtime_stack_frame;

typedef bool lauf_runtime_builtin_impl(const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr,
                                       lauf_runtime_stack_frame* frame_ptr,
                                       lauf_runtime_process*     process);

/// The layout of a type.
typedef struct lauf_asm_layout
{
    size_t size;
    size_t alignment;
} lauf_asm_layout;

#if defined(__cplusplus)
#    define LAUF_ASM_NATIVE_LAYOUT_OF(Type)                                                        \
        lauf_asm_layout                                                                            \
        {                                                                                          \
            sizeof(Type), alignof(Type)                                                            \
        }
#else
#    define LAUF_ASM_NATIVE_LAYOUT_OF(Type)                                                        \
        (lauf_asm_layout)                                                                          \
        {                                                                                          \
            sizeof(Type), alignof(Type)                                                            \
        }
#endif

/// Layout of an array with the specified element layout.
lauf_asm_layout lauf_asm_array_layout(lauf_asm_layout element_layout, size_t element_count);

/// Layout of an aggregate with the specified member types.
lauf_asm_layout lauf_asm_aggregate_layout(const lauf_asm_layout* member_layouts,
                                          size_t                 member_count);

/// A type, which controls load/store operations in memory.
///
/// It consists of a number of fields that can be individually load/stored using the vstack.
typedef struct lauf_asm_type
{
    lauf_asm_layout layout;
    size_t          field_count;
    /// Signature: ptr:void* field_index:uint => value
    /// Builder guarantees that ptr (already dereferenced) and field_index are valid, so it does not
    /// need to be checked.
    lauf_runtime_builtin_impl* load_fn;
    /// Signature: value ptr:void* field_index:uint => _
    /// Builder guarantees that ptr (already dereferenced) and field_index are valid, so it does not
    /// need to be checked.
    lauf_runtime_builtin_impl* store_fn;
    /// The name, used for debugging and some frontends/backends.
    const char* name;
    /// The next type of a builtin library; forms a linked list of types.
    const struct lauf_asm_type* next;
} lauf_asm_type;

/// The type that corresponds to the value stored on the vstack.
extern const lauf_asm_type lauf_asm_type_value;

LAUF_HEADER_END

#endif // LAUF_ASM_TYPE_H_INCLUDED

