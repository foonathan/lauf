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
    switch (fiber->status)
    {
    case lauf_runtime_fiber::ready:
        return new lauf_runtime_stacktrace{fiber->suspension_point.frame_ptr,
                                           fiber->root_function()->insts};
    case lauf_runtime_fiber::suspended:
        return new lauf_runtime_stacktrace{fiber->suspension_point.frame_ptr,
                                           fiber->suspension_point.ip};
    case lauf_runtime_fiber::running:
        assert(p->cur_fiber == fiber);
        return new lauf_runtime_stacktrace{p->regs.frame_ptr, p->regs.ip};
    case lauf_runtime_fiber::done:
        return nullptr;
    }
}

const lauf_asm_function* lauf_runtime_stacktrace_function(lauf_runtime_stacktrace* st)
{
    return st->frame->function;
}

const lauf_asm_inst* lauf_runtime_stacktrace_instruction(lauf_runtime_stacktrace* st)
{
    return st->ip;
}

lauf_runtime_stacktrace* lauf_runtime_stacktrace_parent(lauf_runtime_stacktrace* st)
{
    if (st->frame->prev == nullptr || st->frame->is_root_frame())
    {
        delete st;
        return nullptr;
    }
    else
    {
        st->ip    = st->frame->return_ip - 1;
        st->frame = st->frame->prev;
        return st;
    }
}

void lauf_runtime_destroy_stacktrace(lauf_runtime_stacktrace* st)
{
    delete st;
}

