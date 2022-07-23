// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/vm.hpp>

#include <cassert>
#include <lauf/asm/module.hpp>
#include <lauf/asm/type.h>
#include <lauf/runtime/builtin.h>

namespace
{
bool do_panic(const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr,
              lauf_runtime_stack_frame* frame_ptr, lauf_runtime_process* process)
{
    auto msg = reinterpret_cast<const char*>(vstack_ptr);

    process->frame_ptr = frame_ptr;
    return lauf_runtime_panic(process, ip, msg);
}
#define LAUF_DO_PANIC(Msg)                                                                         \
    [[clang::musttail]] return do_panic(ip, (lauf_runtime_value*)(Msg), frame_ptr, process)

} // namespace

#define LAUF_VM_EXECUTE(Name)                                                                      \
    bool lauf::execute_##Name(const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr,             \
                              lauf_runtime_stack_frame* frame_ptr, lauf_runtime_process* process)

//=== control flow ===//
LAUF_VM_EXECUTE(nop)
{
    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(return_)
{
    ip        = frame_ptr->return_ip;
    frame_ptr = frame_ptr->prev;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(jump)
{
    ip += ip->jump.offset;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(branch_false)
{
    auto condition = vstack_ptr[0].as_uint;
    ++vstack_ptr;

    if (condition == 0)
        ip += ip->jump.offset;
    else
        ++ip;

    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(branch_eq)
{
    auto condition = vstack_ptr[0].as_sint;

    if (condition == 0)
    {
        ip += ip->branch_eq.offset;
        ++vstack_ptr;
    }
    else
        ++ip;

    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(branch_gt)
{
    auto condition = vstack_ptr[0].as_sint;
    ++vstack_ptr;

    if (condition > 0)
        ip += ip->branch_gt.offset;
    else
        ++ip;

    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(panic)
{
    auto msg = lauf_runtime_get_cstr(process, vstack_ptr[0].as_address);
    ++vstack_ptr;

    LAUF_DO_PANIC(msg);
}

LAUF_VM_EXECUTE(exit)
{
    (void)ip;
    (void)frame_ptr;
    (void)vstack_ptr;
    (void)process;
    return true;
}

//=== calls ===//
LAUF_VM_EXECUTE(call_builtin)
{
    auto callee
        = lauf::uncompress_pointer_offset<lauf_runtime_builtin_impl>(&lauf_runtime_builtin_dispatch,
                                                                     ip->call_builtin.offset);

    process->frame_ptr = frame_ptr;
    [[clang::musttail]] return callee(ip, vstack_ptr, frame_ptr, process);
}

LAUF_VM_EXECUTE(call)
{
    auto callee
        = lauf::uncompress_pointer_offset<lauf_asm_function>(frame_ptr->function, ip->call.offset);

    // Check that we have enough space left on the vstack.
    if (auto remaining = vstack_ptr - process->vstack_end; remaining < callee->max_vstack_size)
        LAUF_DO_PANIC("vstack overflow");

    // Check that we have enough space left on the cstack.
    if (reinterpret_cast<unsigned char*>(frame_ptr + 1) >= process->cstack_end)
        LAUF_DO_PANIC("cstack overflow");

    // Create a new stack frame.
    frame_ptr = ::new (frame_ptr->next_frame()) auto(
        lauf_runtime_stack_frame::make_call_frame(callee, process, ip, frame_ptr));

    // And start executing the function.
    ip = callee->insts;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(tail_call)
{
    auto callee
        = lauf::uncompress_pointer_offset<lauf_asm_function>(frame_ptr->function, ip->call.offset);

    // Check that we have enough space left on the vstack.
    if (auto remaining = vstack_ptr - process->vstack_end; remaining < callee->max_vstack_size)
        LAUF_DO_PANIC("vstack overflow");

    // Overwrite the current function, return_ip stays unchanged.
    frame_ptr->function = callee;

    // And start executing the function.
    ip = callee->insts;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(call_indirect)
{
    auto ptr = vstack_ptr[0].as_function_address;
    ++vstack_ptr;

    auto callee = lauf_runtime_get_function_ptr(process, ptr,
                                                {ip->call_indirect.input_count,
                                                 ip->call_indirect.output_count});
    if (callee == nullptr)
        LAUF_DO_PANIC("invalid function address");

    // Check that we have enough space left on the vstack.
    if (auto remaining = vstack_ptr - process->vstack_end; remaining < callee->max_vstack_size)
        LAUF_DO_PANIC("vstack overflow");

    // Check that we have enough space left on the cstack.
    if (reinterpret_cast<unsigned char*>(frame_ptr + 1) >= process->cstack_end)
        LAUF_DO_PANIC("cstack overflow");

    // Create a new stack frame.
    frame_ptr = ::new (frame_ptr->next_frame()) auto(
        lauf_runtime_stack_frame::make_call_frame(callee, process, ip, frame_ptr));

    // And start executing the function.
    ip = callee->insts;
    LAUF_VM_DISPATCH;
}
LAUF_VM_EXECUTE(tail_call_indirect)
{
    auto ptr = vstack_ptr[0].as_function_address;
    ++vstack_ptr;

    auto callee = lauf_runtime_get_function_ptr(process, ptr,
                                                {ip->call_indirect.input_count,
                                                 ip->call_indirect.output_count});
    if (callee == nullptr)
        LAUF_DO_PANIC("invalid function address");

    // Check that we have enough space left on the vstack.
    if (auto remaining = vstack_ptr - process->vstack_end; remaining < callee->max_vstack_size)
        LAUF_DO_PANIC("vstack overflow");

    // Overwrite the current function, return_ip stays unchanged.
    frame_ptr->function = callee;

    // And start executing the function.
    ip = callee->insts;
    LAUF_VM_DISPATCH;
}

//=== value instructions ===//
LAUF_VM_EXECUTE(push)
{
    --vstack_ptr;
    vstack_ptr[0].as_uint = ip->push.value;

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(pushn)
{
    --vstack_ptr;
    vstack_ptr[0].as_uint = ~lauf_uint(ip->push.value);

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(push2)
{
    vstack_ptr[0].as_uint |= lauf_uint(ip->push2.value) << 24;

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(push3)
{
    vstack_ptr[0].as_uint |= lauf_uint(ip->push2.value) << 48;

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(global_addr)
{
    --vstack_ptr;
    vstack_ptr[0].as_address.allocation = ip->global_addr.value;
    vstack_ptr[0].as_address.offset     = 0;
    vstack_ptr[0].as_address.generation = 0; // Always true for globals.

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(function_addr)
{
    auto fn = lauf::uncompress_pointer_offset<lauf_asm_function>(frame_ptr->function,
                                                                 ip->function_addr.offset);

    --vstack_ptr;
    vstack_ptr[0].as_function_address.index        = fn->function_idx;
    vstack_ptr[0].as_function_address.input_count  = fn->sig.input_count;
    vstack_ptr[0].as_function_address.output_count = fn->sig.output_count;

    ++ip;
    LAUF_VM_DISPATCH;
}

//=== stack manipulation ===//
LAUF_VM_EXECUTE(pop)
{
    // Move everything above one over.
    std::memmove(vstack_ptr + 1, vstack_ptr, ip->pop.idx * sizeof(lauf_runtime_value));
    // Remove the now duplicate top value.
    ++vstack_ptr;

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(pop_top)
{
    assert(ip->pop_top.idx == 0);
    ++vstack_ptr;

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(pick)
{
    auto value = vstack_ptr[ip->pick.idx];
    --vstack_ptr;
    vstack_ptr[0] = value;

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(dup)
{
    assert(ip->dup.idx == 0);
    --vstack_ptr;
    vstack_ptr[0] = vstack_ptr[1];

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(roll)
{
    // Remember the value as we're about to overwrite it.
    auto value = vstack_ptr[ip->roll.idx];
    // Move everything above one over.
    std::memmove(vstack_ptr + 1, vstack_ptr, ip->pop.idx * sizeof(lauf_runtime_value));
    // Replace the now duplicate top value.
    vstack_ptr[0] = value;

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(swap)
{
    assert(ip->swap.idx == 1);
    auto tmp      = vstack_ptr[0];
    vstack_ptr[0] = vstack_ptr[1];
    vstack_ptr[1] = tmp;

    ++ip;
    LAUF_VM_DISPATCH;
}

//=== memory ===//
LAUF_VM_EXECUTE(local_alloc)
{
    // The builder has taken care of ensuring alignment.
    assert(ip->local_alloc.alignment == alignof(void*));
    assert(lauf::is_aligned(frame_ptr->next_frame(), alignof(void*)));

    auto memory = frame_ptr->next_frame();
    frame_ptr->next_offset += ip->local_alloc.size;

    lauf::allocation alloc;
    alloc.ptr        = memory;
    alloc.size       = ip->local_alloc.size;
    alloc.source     = allocation_source::local_memory;
    alloc.status     = allocation_status::allocated;
    alloc.generation = process->alloc_generation;
    process->allocations.push_back(*process->vm, alloc);

    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(local_free)
{
    for (auto i = 0u; i != ip->local_free.value; ++i)
    {
        auto index                         = frame_ptr->first_local_alloc + i;
        process->allocations[index].status = allocation_status::freed;
    }

    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(deref_const)
{
    auto address = vstack_ptr[0].as_address;

    auto ptr = lauf_runtime_get_const_ptr(process, address,
                                          {ip->deref_const.size, ip->deref_const.alignment});
    if (ptr == nullptr)
        LAUF_DO_PANIC("invalid address");

    vstack_ptr[0].as_native_ptr = const_cast<void*>(ptr);

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(deref_mut)
{
    auto address = vstack_ptr[0].as_address;

    auto ptr
        = lauf_runtime_get_mut_ptr(process, address, {ip->deref_mut.size, ip->deref_mut.alignment});
    if (ptr == nullptr)
        LAUF_DO_PANIC("invalid address");

    vstack_ptr[0].as_native_ptr = ptr;

    ++ip;
    LAUF_VM_DISPATCH;
}

