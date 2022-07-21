// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_VM_HPP_INCLUDED
#define SRC_LAUF_VM_HPP_INCLUDED

#include <lauf/vm.h>

#include <lauf/asm/instruction.hpp>
#include <lauf/runtime/process.hpp>
#include <lauf/runtime/value.h>
#include <lauf/support/arena.hpp>

struct lauf_vm : lauf::intrinsic_arena<lauf_vm>
{
    lauf_vm_panic_handler panic_handler;

    // Grows up.
    unsigned char* cstack_base;
    std::size_t    cstack_size;

    // Grows down.
    lauf_runtime_value* vstack_base;
    std::size_t         vstack_size;

    explicit lauf_vm(lauf::arena_key key, lauf_vm_options options)
    : lauf::intrinsic_arena<lauf_vm>(key), panic_handler(options.panic_handler),
      cstack_size(options.cstack_size_in_bytes), vstack_size(options.vstack_size_in_elements)
    {
        // We allocate the stacks using new, as unlike the arena, their memory should not be freed
        // between executions.
        cstack_base = static_cast<unsigned char*>(::operator new(cstack_size));

        // It grows down, so the base is at the end.
        vstack_base = static_cast<lauf_runtime_value*>(
                          ::operator new(vstack_size * sizeof(lauf_runtime_value)))
                      + vstack_size;
    }

    ~lauf_vm()
    {
        ::operator delete(cstack_base);
        ::operator delete(vstack_base - vstack_size);
    }
};

//=== execute ===//
namespace lauf
{
using dispatch_fn = bool (*)(const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr,
                             lauf_runtime_stack_frame* frame_ptr, lauf_runtime_process* process);

#define LAUF_ASM_INST(Name, Type)                                                                  \
    bool execute_##Name(const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr,                   \
                        lauf_runtime_stack_frame* frame_ptr, lauf_runtime_process* process);
#include <lauf/asm/instruction.def.hpp>
#undef LAUF_ASM_INST

constexpr dispatch_fn dispatch[] = {
#define LAUF_ASM_INST(Name, Type) &execute_##Name,
#include <lauf/asm/instruction.def.hpp>
#undef LAUF_ASM_INST
};

#define LAUF_VM_DISPATCH                                                                           \
    [[clang::musttail]] return lauf::dispatch[std::size_t(ip->op())](ip, vstack_ptr, frame_ptr,    \
                                                                     process)

inline bool execute(const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr,
                    lauf_runtime_stack_frame* frame_ptr, lauf_runtime_process* process)
{
    LAUF_VM_DISPATCH;
}
} // namespace lauf

#endif // SRC_LAUF_VM_HPP_INCLUDED

