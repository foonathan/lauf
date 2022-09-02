// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_RUNTIME_PROCESS_HPP_INCLUDED
#define SRC_LAUF_RUNTIME_PROCESS_HPP_INCLUDED

#include <lauf/runtime/process.h>

#include <lauf/runtime/memory.hpp>
#include <lauf/runtime/stack.hpp>
#include <lauf/support/array.hpp>

typedef struct lauf_asm_function lauf_asm_function;
typedef union lauf_asm_inst      lauf_asm_inst;
typedef struct lauf_asm_global   lauf_asm_global;
typedef struct lauf_vm           lauf_vm;

struct lauf_runtime_process
{
    // The VM that is executing the process.
    lauf_vm* vm = nullptr;

    lauf::vstack vstack;
    lauf::cstack cstack;
    lauf::memory memory;

    // The dummy frame for call stacks -- this is only lazily updated
    // It needs to be valid when calling a builtin or panicing.
    lauf_runtime_stack_frame callstack_leaf_frame;

    // The program that is running.
    const lauf_asm_program* program = nullptr;

    std::size_t remaining_steps;
};

#endif // SRC_LAUF_RUNTIME_PROCESS_HPP_INCLUDED

