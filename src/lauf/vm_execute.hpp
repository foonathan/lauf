// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_VM_EXECUTE_HPP_INCLUDED
#define SRC_LAUF_VM_EXECUTE_HPP_INCLUDED

#include <lauf/asm/instruction.hpp>
#include <lauf/config.h>
#include <lauf/runtime/builtin.h>

namespace lauf
{
extern lauf_runtime_builtin_impl* const dispatch[std::size_t(asm_op::count)];

#define LAUF_VM_DISPATCH                                                                           \
    LAUF_TAIL_CALL return lauf::dispatch[std::size_t(ip->op())](ip, vstack_ptr, frame_ptr, process)

inline bool execute(const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr,
                    lauf_runtime_stack_frame* frame_ptr, lauf_runtime_process* process)
{
    LAUF_VM_DISPATCH;
}
} // namespace lauf

inline bool lauf_runtime_builtin_dispatch(const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr,
                                          lauf_runtime_stack_frame* frame_ptr,
                                          lauf_runtime_process*     process)
{
    ++ip;
    LAUF_VM_DISPATCH;
}

#endif // SRC_LAUF_VM_EXECUTE_HPP_INCLUDED

