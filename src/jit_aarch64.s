; Copyright (C) 2022 Jonathan Müller and lauf contributors
; SPDX-License-Identifier: BSL-1.0

.global lauf_jit_call_vm_function

; INPUT
; =====
; Values: s1 s2 s3 ... sn | x0 x1 ... x7
;             stack           registers
; x8 - JIT state
; x16 (IP0) - lauf_vm_instruction* to the call instruction that should be executed
; x17 (IP1) - lower half is input count, upper half is output count
;
; SUCCESS
; =======
; Values: s1 s2 s3 ... sn | x0 x1 ... x7
;             stack           registers
; x8 - JIT state
;
; PANIC
; =====
; x8 - null
lauf_jit_call_vm_function:
    ; Push all argument registers onto the stack as well.
    ; This needs to happen first as it needs to be directly adjacent to the other values on the stack.
    stp x0, x1, [sp, -16]!
    stp x2, x3, [sp, -16]!
    stp x4, x5, [sp, -16]!
    stp x6, x7, [sp, -16]!

    ; Now we can use the stack to save registers, lr and x17.
    stp lr, x17, [sp, -16]!

    ; Set the actual arguments for the dispatch function.
    ; ip is given in x16.
    mov x0, x16
    ; vstack_ptr points to the first argument of the stack.
    add x1, sp, 16 ; skip over lr and x17
    add x1, sp, 

