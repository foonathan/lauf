// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/runtime/stacktrace.h>

#include <lauf/runtime/process.hpp>

lauf_runtime_stacktrace* lauf_runtime_get_stacktrace(lauf_runtime_process p)
{
    // A stacktrace is simply the frame pointer reinterpret-casted.
    // Code that exposes the process needs to set it correctly.
    return reinterpret_cast<lauf_runtime_stacktrace*>(p.frame_ptr);
}

lauf_asm_function* lauf_runtime_stacktrace_function(lauf_runtime_stacktrace* bt)
{
    // The function is always one level back.
    return reinterpret_cast<lauf::stack_frame*>(bt)->prev->function;
}

const void* lauf_runtime_stacktrace_address(lauf_runtime_stacktrace* bt)
{
    // The return ip is at the current level, it points inside the previous function.
    return reinterpret_cast<lauf::stack_frame*>(bt)->return_ip;
}

lauf_runtime_stacktrace* lauf_runtime_stacktrace_parent(lauf_runtime_stacktrace* bt)
{
    auto frame_ptr = reinterpret_cast<lauf::stack_frame*>(bt)->prev;
    if (frame_ptr->prev == nullptr)
        // We're done when we've reached the root frame.
        return nullptr;
    return reinterpret_cast<lauf_runtime_stacktrace*>(frame_ptr);
}

