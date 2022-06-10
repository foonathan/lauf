// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_IMPL_BUILTIN_HPP_INCLUDED
#define SRC_LAUF_IMPL_BUILTIN_HPP_INCLUDED

#include <cstdint>
#include <lauf/builtin.h>
#include <lauf/bytecode.hpp>

//=== lauf_builtin_dispatch ===//
#if LAUF_HAS_TAIL_CALL
namespace lauf
{
extern lauf_builtin_function* const inst_fns[size_t(bc_op::_count)];
}

LAUF_INLINE bool lauf_builtin_finish(lauf_vm_instruction* ip, lauf_value* vstack_ptr,
                                     void* frame_ptr, lauf_vm_process process)
{
    auto ipv = reinterpret_cast<std::uintptr_t>(ip);
    if ((ipv & 0b1) != 0)
        // Builtin is being called from JIT compiled code, and need to return.
        return true;
    else
        // Builtin is being called from regular code, continue with instruction after it.
        LAUF_TAIL_CALL return lauf::inst_fns[size_t(ip->tag.op)](ip, vstack_ptr, frame_ptr,
                                                                 process);
}
#else
LAUF_INLINE bool lauf_builtin_finish(lauf_vm_instruction*, lauf_value*, void*, lauf_vm_process)
{
    // We terminate the call here, so we don't get a stack overflow.
    return true;
}
#endif

//=== lauf_builtin_panic ===//
namespace lauf
{
LAUF_NOINLINE_IF_TAIL bool do_panic(lauf_vm_instruction* ip, lauf_value* vstack_ptr,
                                    void* frame_ptr, lauf_vm_process process);
}

LAUF_INLINE bool lauf_builtin_panic(lauf_vm_instruction* ip, lauf_value* vstack_ptr,
                                    void* frame_ptr, lauf_vm_process process)
{
    // call_builtin has already incremented ip, so undo it to get the location.
    LAUF_TAIL_CALL return lauf::do_panic(ip - 1, vstack_ptr, frame_ptr, process);
}

#endif // SRC_LAUF_IMPL_BUILTIN_HPP_INCLUDED

