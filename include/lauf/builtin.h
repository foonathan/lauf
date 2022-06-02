// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_BUILTIN_H_INCLUDED
#define LAUF_BUILTIN_H_INCLUDED

#include <lauf/config.h>
#include <lauf/vm.h>

LAUF_HEADER_START

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
bool lauf_builtin_dispatch(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr,
                           lauf_vm_process process);

/// This function must be called at the end of a builtin that panics.
bool lauf_builtin_panic(lauf_vm_process process, lauf_vm_instruction* ip, void* frame_ptr,
                        const char* message);

// Creates a builtin that consumes one argument to produce N.
#define LAUF_BUILTIN_UNARY_OPERATION(Name, N, ...)                                                 \
    static bool Name##_fn(lauf_vm_instruction* ip, lauf_value* _vstack_ptr, void* frame_ptr,       \
                          lauf_vm_process process)                                                 \
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

// Creates a builtin that consumes two arguments to produce N.
#define LAUF_BUILTIN_BINARY_OPERATION(Name, N, ...)                                                \
    static bool Name##_fn(lauf_vm_instruction* ip, lauf_value* _vstack_ptr, void* frame_ptr,       \
                          lauf_vm_process process)                                                 \
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

LAUF_HEADER_END

#endif // LAUF_BUILTIN_H_INCLUDED

