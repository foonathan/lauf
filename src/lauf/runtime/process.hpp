// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_RUNTIME_PROCESS_HPP_INCLUDED
#define SRC_LAUF_RUNTIME_PROCESS_HPP_INCLUDED

#include <lauf/runtime/process.h>

#include <lauf/asm/instruction.hpp>
#include <lauf/asm/type.h>
#include <lauf/runtime/value.h>
#include <lauf/support/array.hpp>

typedef struct lauf_asm_function lauf_asm_function;
typedef union lauf_asm_inst      lauf_asm_inst;
typedef struct lauf_asm_global   lauf_asm_global;
typedef struct lauf_vm           lauf_vm;

//=== stack frame ===//
struct lauf_runtime_stack_frame
{
    // The current function.
    const lauf_asm_function* function;
    // The return address to jump to when the call finishes.
    const lauf_asm_inst* return_ip;
    // The allocation of the first local_alloc (only meaningful if the function has any)
    uint16_t first_local_alloc;
    // The generation of the local variables.
    uint8_t local_generation;
    // The offset from this where the next frame can be put, i.e. after the local allocs.
    uint32_t next_offset;
    // The previous stack frame.
    lauf_runtime_stack_frame* prev;

    static lauf_runtime_stack_frame make_trampoline_frame(const lauf_asm_function* fn)
    {
        return {fn, nullptr, 0, 0, sizeof(lauf_runtime_stack_frame), nullptr};
    }
    static lauf_runtime_stack_frame make_call_frame(const lauf_asm_function*    callee,
                                                    const lauf_runtime_process* process,
                                                    const lauf_asm_inst*        ip,
                                                    lauf_runtime_stack_frame*   frame_ptr);

    void assign_callstack_leaf_frame(const lauf_asm_inst* ip, lauf_runtime_stack_frame* frame_ptr)
    {
        return_ip = ip + 1;
        prev      = frame_ptr;
    }

    bool is_trampoline_frame() const
    {
        return prev == nullptr;
    }

    bool is_root_frame() const
    {
        return prev->is_trampoline_frame();
    }

    void* next_frame()
    {
        return reinterpret_cast<unsigned char*>(this) + next_offset;
    }
};
static_assert(alignof(lauf_runtime_stack_frame) == alignof(void*));

//=== allocation ===//
namespace lauf
{
enum class allocation_source : std::uint8_t
{
    static_const_memory,
    static_mut_memory,
    local_memory,
};

constexpr bool is_const(allocation_source source)
{
    switch (source)
    {
    case allocation_source::static_const_memory:
        return true;
    case allocation_source::static_mut_memory:
    case allocation_source::local_memory:
        return false;
    }
}

enum allocation_status : std::uint8_t
{
    allocated,
    freed,
};

constexpr bool is_usable(allocation_status status)
{
    switch (status)
    {
    case allocated:
        return true;
    case freed:
        return false;
    }
}

struct allocation
{
    void*             ptr;
    std::uint32_t     size;
    allocation_source source;
    allocation_status status;
    std::uint8_t      generation;

    constexpr void* unchecked_offset(std::uint32_t offset) const
    {
        return static_cast<unsigned char*>(ptr) + offset;
    }
};
static_assert(sizeof(allocation) == 2 * sizeof(void*));
} // namespace lauf

struct lauf_runtime_process
{
    // The VM that is executing the process.
    lauf_vm*            vm         = nullptr;
    lauf_runtime_value* vstack_end = nullptr;
    unsigned char*      cstack_end = nullptr;

    // The dummy frame for call stacks -- this is only lazily updated
    // It needs to be valid when calling a builtin or panicing.
    lauf_runtime_stack_frame callstack_leaf_frame;

    // The program that is running.
    const lauf_asm_program* program = nullptr;

    // The allocations of the process.
    lauf::array<lauf::allocation> allocations;
    std::uint8_t                  alloc_generation = 0;

    lauf::allocation* get_allocation(std::uint32_t index)
    {
        if (index >= allocations.size())
            return nullptr;
        else
            return &allocations[index];
    }

    void try_free_allocations()
    {
        if (allocations.empty() || allocations.back().status != lauf::allocation_status::freed)
            // We only remove from the back.
            return;

        do
        {
            allocations.pop_back();
        } while (!allocations.empty()
                 && allocations.back().status == lauf::allocation_status::freed);

        // Since we changed something, we need to increment the generation.
        ++alloc_generation;
    }
};

inline lauf_runtime_stack_frame lauf_runtime_stack_frame::make_call_frame(
    const lauf_asm_function* callee, const lauf_runtime_process* process, const lauf_asm_inst* ip,
    lauf_runtime_stack_frame* frame_ptr)
{
    auto alloc_idx = std::uint16_t(process->allocations.size());
    return {callee,
            ip + 1,
            alloc_idx,
            process->alloc_generation,
            sizeof(lauf_runtime_stack_frame),
            frame_ptr};
}

namespace lauf
{
inline const void* checked_offset(lauf::allocation alloc, lauf_runtime_address addr,
                                  lauf_asm_layout layout)
{
    if (!lauf::is_usable(alloc.status) || (alloc.generation & 0b11) != addr.generation)
        return nullptr;

    if (addr.offset + layout.size > alloc.size)
        return nullptr;

    auto ptr = alloc.unchecked_offset(addr.offset);
    if (!lauf::is_aligned(ptr, layout.alignment))
        return nullptr;

    return ptr;
}

inline const void* checked_offset(lauf::allocation alloc, lauf_runtime_address addr)
{
    if (!lauf::is_usable(alloc.status) || (alloc.generation & 0b11) != addr.generation)
        return nullptr;

    if (addr.offset >= alloc.size)
        return nullptr;

    return alloc.unchecked_offset(addr.offset);
}
} // namespace lauf

#endif // SRC_LAUF_RUNTIME_PROCESS_HPP_INCLUDED

