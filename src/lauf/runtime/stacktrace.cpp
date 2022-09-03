// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/runtime/stacktrace.h>

#include <lauf/asm/instruction.hpp>
#include <lauf/runtime/process.hpp>

struct lauf_runtime_stacktrace
{
    const lauf_runtime_stack_frame* frame;
    const lauf_asm_inst*            ip;
};

lauf_runtime_stacktrace* lauf_runtime_get_stacktrace(lauf_runtime_process*     p,
                                                     const lauf_runtime_fiber* fiber)
{
    if (fiber == p->cur_fiber)
        return new lauf_runtime_stacktrace{p->regs.frame_ptr, p->regs.ip};
    else
        return new lauf_runtime_stacktrace{fiber->regs.frame_ptr, fiber->regs.ip};
}

const lauf_asm_function* lauf_runtime_stacktrace_function(lauf_runtime_stacktrace* bt)
{
    return bt->frame->function;
}

const lauf_asm_inst* lauf_runtime_stacktrace_instruction(lauf_runtime_stacktrace* bt)
{
    return bt->ip;
}

lauf_runtime_stacktrace* lauf_runtime_stacktrace_parent(lauf_runtime_stacktrace* bt)
{
    assert(bt->frame->prev != nullptr);
    if (bt->frame->is_root_frame())
    {
        delete bt;
        return nullptr;
    }
    else
    {
        bt->ip    = bt->frame->return_ip - 1;
        bt->frame = bt->frame->prev;
        return bt;
    }
}

