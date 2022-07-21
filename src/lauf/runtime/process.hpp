// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_RUNTIME_PROCESS_HPP_INCLUDED
#define SRC_LAUF_RUNTIME_PROCESS_HPP_INCLUDED

#include <lauf/runtime/process.h>

#include <lauf/support/array.hpp>

typedef struct lauf_asm_function lauf_asm_function;
typedef union lauf_asm_inst      lauf_asm_inst;
typedef struct lauf_asm_global   lauf_asm_global;
typedef struct lauf_vm           lauf_vm;

//=== stack frame ===//
namespace lauf
{
struct stack_frame
{
    // The current function.
    const lauf_asm_function* function;
    // The return address to jump to when the call finishes.
    const lauf_asm_inst* return_ip;
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

//=== allocation ===//
namespace lauf
{
enum class allocation_source : std::uint8_t
{
    static_const_memory,
    static_mut_memory,
};

constexpr bool is_const(allocation_source source)
{
    switch (source)
    {
    case allocation_source::static_const_memory:
        return true;
    case allocation_source::static_mut_memory:
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
    lauf_vm* vm = nullptr;
    // The program that is running.
    const lauf_asm_program* program = nullptr;

    // The current frame pointer -- this is only lazily updated.
    // Whenever the process is exposed, it needs to point to a dummy stack frame
    // whoese return_ip is the current ip and prev points to the actual stack frame.
    lauf::stack_frame* frame_ptr = nullptr;
    // The current vstack pointer -- this is also only lazily updated.
    lauf_runtime_value* vstack_ptr = nullptr;

    // The allocations of the process.
    lauf::array<lauf::allocation> allocations;

    lauf::allocation* get_allocation(std::uint32_t index)
    {
        if (index >= allocations.size())
            return nullptr;
        else
            return &allocations[index];
    }
};

#endif // SRC_LAUF_RUNTIME_PROCESS_HPP_INCLUDED

