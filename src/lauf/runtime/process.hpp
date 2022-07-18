// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_RUNTIME_PROCESS_HPP_INCLUDED
#define SRC_LAUF_RUNTIME_PROCESS_HPP_INCLUDED

#include <lauf/runtime/process.h>

#include <lauf/asm/instruction.hpp>
#include <lauf/runtime/stacktrace.h>
#include <lauf/vm.h>

namespace lauf
{
struct stack_frame
{
    // The current function.
    lauf_asm_function* function;
    // The return address to jump to when the call finishes.
    asm_inst* return_ip;
    // The previous stack frame.
    stack_frame* prev;

    bool is_trampoline_frame() const
    {
        return prev == nullptr;
    }

    bool is_root_frame() const
    {
        return prev->is_trampoline_frame();
    }
};
} // namespace lauf

struct lauf_runtime_process
{
    // The VM that is executing the process.
    lauf_vm* vm = nullptr;

    // The current frame pointer -- this is only lazily updated.
    // Whenever the process is exposed, it needs to point to a dummy stack frame
    // whoese return_ip is the current ip and prev points to the actual stack frame.
    lauf::stack_frame* frame_ptr = nullptr;
};

#endif // SRC_LAUF_RUNTIME_PROCESS_HPP_INCLUDED

