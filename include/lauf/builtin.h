// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_BUILTIN_H_INCLUDED
#define LAUF_BUILTIN_H_INCLUDED

#include <lauf/config.h>
#include <lauf/vm.h>

LAUF_HEADER_START

#define LAUF_BUILTIN_SECTION LAUF_SECTION("text.lauf_builtin")

/// The signature of a builtin function.
/// vstack_ptr is the value stack pointer, vstack_ptr[0] is the top of the stack, vstack_ptr[1] the
/// item below that and so on. It can be incremented/decremented as necessary. The other arguments
/// must be forwarded unchanged.
typedef bool lauf_builtin_function(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr,
                                   lauf_vm_process process);

typedef struct lauf_builtin
{
    lauf_signature         signature;
    lauf_builtin_function* impl;
} lauf_builtin;

/// This function must be tail-called at the end of the builtin.
LAUF_BUILTIN_SECTION bool lauf_builtin_dispatch(lauf_vm_instruction* ip, lauf_value* vstack_ptr,
                                                void* frame_ptr, lauf_vm_process process);

/// This function must be called at the end of a builtin that panics.
/// vstack_ptr->as_native_ptr is the `const char*` message.
LAUF_BUILTIN_SECTION bool lauf_builtin_panic(lauf_vm_instruction* ip, lauf_value* vstack_ptr,
                                             void* frame_ptr, lauf_vm_process process);

/// Creates a builtin that consumes one argument to produce N.
#define LAUF_BUILTIN_UNARY_OPERATION(Name, N, ...)                                                 \
    LAUF_BUILTIN_SECTION static bool Name##_fn(lauf_vm_instruction* ip, lauf_value* _vstack_ptr,   \
                                               void* frame_ptr, lauf_vm_process process)           \
    {                                                                                              \
        lauf_value value = _vstack_ptr[0];                                                         \
        _vstack_ptr += 1 - N;                                                                      \
        lauf_value* result = _vstack_ptr;                                                          \
        __VA_ARGS__                                                                                \
        LAUF_TAIL_CALL return lauf_builtin_dispatch(ip, _vstack_ptr, frame_ptr, process);          \
    }                                                                                              \
    lauf_builtin Name(void)                                                                        \
    {                                                                                              \
        return {{1, N}, &Name##_fn};                                                               \
    }

/// Creates a builtin that consumes two arguments to produce N.
#define LAUF_BUILTIN_BINARY_OPERATION(Name, N, ...)                                                \
    LAUF_BUILTIN_SECTION static bool Name##_fn(lauf_vm_instruction* ip, lauf_value* _vstack_ptr,   \
                                               void* frame_ptr, lauf_vm_process process)           \
    {                                                                                              \
        lauf_value lhs = _vstack_ptr[1];                                                           \
        lauf_value rhs = _vstack_ptr[0];                                                           \
        _vstack_ptr += 2 - N;                                                                      \
        lauf_value* result = _vstack_ptr;                                                          \
        __VA_ARGS__                                                                                \
        LAUF_TAIL_CALL return lauf_builtin_dispatch(ip, _vstack_ptr, frame_ptr, process);          \
    }                                                                                              \
    lauf_builtin Name(void)                                                                        \
    {                                                                                              \
        return {{2, N}, &Name##_fn};                                                               \
    }

/// Panics within a builtin operation function.
#define LAUF_BUILTIN_OPERATION_PANIC(Msg)                                                          \
    do                                                                                             \
    {                                                                                              \
        result->as_native_ptr = (Msg);                                                             \
        LAUF_TAIL_CALL return lauf_builtin_panic(ip, _vstack_ptr, frame_ptr, process);             \
    } while (0)

LAUF_HEADER_END

#endif // LAUF_BUILTIN_H_INCLUDED

