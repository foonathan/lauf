// Copyright(C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/vm.hpp>

#include <cassert>
#include <lauf/asm/builder.h>
#include <lauf/asm/module.hpp>
#include <lauf/runtime/builtin.h>
#include <lauf/vm_execute.hpp>
#include <utility>

//=== execute ===//
#define LAUF_VM_EXECUTE(Name)                                                                      \
    static bool execute_##Name(const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr,            \
                               lauf_runtime_stack_frame* frame_ptr, lauf_runtime_process* process)

#define LAUF_ASM_INST(Name, Type)                                                                  \
    static bool execute_##Name(const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr,            \
                               lauf_runtime_stack_frame* frame_ptr,                                \
                               lauf_runtime_process*     process);
#include <lauf/asm/instruction.def.hpp>
#undef LAUF_ASM_INST

lauf_runtime_builtin_impl* const lauf::dispatch[] = {
#define LAUF_ASM_INST(Name, Type) &execute_##Name,
#include <lauf/asm/instruction.def.hpp>
#undef LAUF_ASM_INST
};

//=== helper functions ===//
// We move expensive calls into a separate function that is tail called.
// That way the the hot path contains no function calls, so the compiler doesn't spill stuff.
namespace
{
LAUF_NOINLINE bool do_panic(const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr,
                            lauf_runtime_stack_frame* frame_ptr, lauf_runtime_process* process)
{
    auto msg      = reinterpret_cast<const char*>(vstack_ptr);
    process->regs = {ip, vstack_ptr, frame_ptr};
    return lauf_runtime_panic(process, msg);
}
#define LAUF_DO_PANIC(Msg)                                                                         \
    LAUF_TAIL_CALL return do_panic(ip, (lauf_runtime_value*)(Msg), frame_ptr, process)

LAUF_NOINLINE bool allocate_more_vstack_space(const lauf_asm_inst*      ip,
                                              lauf_runtime_value*       vstack_ptr,
                                              lauf_runtime_stack_frame* frame_ptr,
                                              lauf_runtime_process*     process)
{
    process->cur_fiber->vstack.grow(process->vm->page_allocator, vstack_ptr);
    if (LAUF_UNLIKELY(process->cur_fiber->vstack.capacity() > process->vm->max_vstack_size))
        LAUF_DO_PANIC("vstack overflow");

    LAUF_VM_DISPATCH;
}

LAUF_NOINLINE bool allocate_more_cstack_space(const lauf_asm_inst*      ip,
                                              lauf_runtime_value*       vstack_ptr,
                                              lauf_runtime_stack_frame* frame_ptr,
                                              lauf_runtime_process*     process)
{
    process->cur_fiber->cstack.grow(process->vm->page_allocator, frame_ptr);
    if (LAUF_UNLIKELY(process->cur_fiber->cstack.capacity() > process->vm->max_cstack_size))
        LAUF_DO_PANIC("cstack overflow");

    LAUF_VM_DISPATCH;
}

LAUF_NOINLINE bool grow_allocation_array(const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr,
                                         lauf_runtime_stack_frame* frame_ptr,
                                         lauf_runtime_process*     process)
{
    process->memory.grow(process->vm->page_allocator);
    LAUF_VM_DISPATCH;
}
} // namespace

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
    if (LAUF_UNLIKELY(process == nullptr))
        // During constant folding, we don't have a process, so check first.
        // We also don't have fibers, so just return.
        return true;

    auto cur_fiber = process->cur_fiber;
    auto new_fiber = lauf::get_fiber(process, cur_fiber->parent);

    cur_fiber->state = lauf_runtime_fiber::done;

    if (new_fiber == nullptr || new_fiber->state == lauf_runtime_fiber::done)
    {
        // We don't have a parent (anymore?), return to lauf_runtime_call().
        process->regs      = {nullptr, nullptr, nullptr};
        process->cur_fiber = nullptr;
        return true;
    }
    else
    {
        // Transfer values from our vstack.
        if (auto argument_count = std::uint8_t(cur_fiber->vstack.base() - vstack_ptr);
            LAUF_UNLIKELY(!new_fiber->transfer_arguments(argument_count, vstack_ptr)))
            LAUF_DO_PANIC("mismatched signature for fiber resume");

        // Switch to parent fiber.
        assert(new_fiber->state == lauf_runtime_fiber::suspended);
        auto regs          = new_fiber->resume();
        process->cur_fiber = new_fiber;

        ip         = regs.ip;
        vstack_ptr = regs.vstack_ptr;
        frame_ptr  = regs.frame_ptr;

        ++ip;
        LAUF_VM_DISPATCH;
    }
}

//=== calls ===//
LAUF_VM_EXECUTE(call_builtin)
{
    process->regs = {ip, vstack_ptr, frame_ptr};
    LAUF_TAIL_CALL return execute_call_builtin_no_frame(ip, vstack_ptr, frame_ptr, process);
}

LAUF_VM_EXECUTE(call_builtin_no_frame)
{
    auto callee
        = lauf::uncompress_pointer_offset<lauf_runtime_builtin_impl>(&lauf_runtime_builtin_dispatch,
                                                                     ip->call_builtin_no_frame
                                                                         .offset);

    LAUF_TAIL_CALL return callee(ip, vstack_ptr, frame_ptr, process);
}

LAUF_VM_EXECUTE(call)
{
    auto callee
        = lauf::uncompress_pointer_offset<lauf_asm_function>(frame_ptr->function, ip->call.offset);

    // Check that we have enough space left on the vstack.
    if (auto remaining = vstack_ptr - process->cur_fiber->vstack.limit();
        LAUF_UNLIKELY(remaining < callee->max_vstack_size))
        LAUF_TAIL_CALL return allocate_more_vstack_space(ip, vstack_ptr, frame_ptr, process);

    // Create a new stack frame.
    auto new_frame = process->cur_fiber->cstack.new_call_frame(frame_ptr, callee, ip);
    if (LAUF_UNLIKELY(new_frame == nullptr))
        LAUF_TAIL_CALL return allocate_more_cstack_space(ip, vstack_ptr, frame_ptr, process);

    // And start executing the function.
    frame_ptr = new_frame;
    ip        = callee->insts;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(call_indirect)
{
    auto ptr    = vstack_ptr[0].as_function_address;
    auto callee = lauf_runtime_get_function_ptr(process, ptr,
                                                {ip->call_indirect.input_count,
                                                 ip->call_indirect.output_count});
    if (LAUF_UNLIKELY(callee == nullptr))
        LAUF_DO_PANIC("invalid function address");

    // Check that we have enough space left on the vstack.
    if (auto remaining = vstack_ptr - process->cur_fiber->vstack.limit();
        LAUF_UNLIKELY(remaining < callee->max_vstack_size))
        LAUF_TAIL_CALL return allocate_more_vstack_space(ip, vstack_ptr, frame_ptr, process);

    // Create a new stack frame.
    auto new_frame = process->cur_fiber->cstack.new_call_frame(frame_ptr, callee, ip);
    if (LAUF_UNLIKELY(new_frame == nullptr))
        LAUF_TAIL_CALL return allocate_more_cstack_space(ip, vstack_ptr, frame_ptr, process);

    // Only modify the vstack_ptr now, when we don't recurse back.
    ++vstack_ptr;

    // And start executing the function.
    frame_ptr = new_frame;
    ip        = callee->insts;
    LAUF_VM_DISPATCH;
}

//=== fiber instructions ===//
LAUF_VM_EXECUTE(fiber_create)
{
    auto callee = lauf::uncompress_pointer_offset<lauf_asm_function>(frame_ptr->function,
                                                                     ip->fiber_create.offset);
    auto fiber  = lauf_runtime_fiber::create(process, callee);

    --vstack_ptr;
    vstack_ptr[0].as_address = fiber->handle();

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(fiber_resume)
{
    auto handle = vstack_ptr[ip->fiber_resume.input_count].as_address;
    auto fiber  = lauf::get_fiber(process, handle);
    if (LAUF_UNLIKELY(fiber == nullptr
                      || (fiber->state != lauf_runtime_fiber::suspended
                          && fiber->state != lauf_runtime_fiber::ready)))
        LAUF_DO_PANIC("invalid fiber handle");

    // Transfer values from our vstack.
    if (auto argument_count = ip->fiber_resume.input_count;
        LAUF_UNLIKELY(!fiber->transfer_arguments(argument_count, vstack_ptr)))
        LAUF_DO_PANIC("mismatched signature for fiber resume");

    // We resume the fiber and set its parent.
    process->cur_fiber->suspend({ip, vstack_ptr, frame_ptr}, ip->fiber_resume.output_count);
    auto regs          = fiber->resume_by(process->cur_fiber);
    process->cur_fiber = fiber;

    ip         = regs.ip;
    vstack_ptr = regs.vstack_ptr;
    frame_ptr  = regs.frame_ptr;

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(fiber_suspend)
{
    assert(process->cur_fiber->state == lauf_runtime_fiber::running);
    auto cur_fiber = process->cur_fiber;

    if (LAUF_UNLIKELY(!cur_fiber->has_resumer()))
    {
        // We're suspending the main fiber, so return instead.
        cur_fiber->suspend({ip, vstack_ptr, frame_ptr}, ip->fiber_suspend.output_count);
        process->cur_fiber = nullptr;
        return true;
    }
    else
    {
        auto new_fiber = lauf::get_fiber(process, cur_fiber->parent);
        if (LAUF_UNLIKELY(new_fiber == nullptr))
            LAUF_DO_PANIC("cannot suspend to destroyed parent");

        // Transfer values from our vstack.
        if (auto argument_count = ip->fiber_suspend.input_count;
            LAUF_UNLIKELY(!new_fiber->transfer_arguments(argument_count, vstack_ptr)))
            LAUF_DO_PANIC("mismatched signature for fiber resume");

        // We resume the parent but without setting its parent (asymmetric).
        cur_fiber->suspend({ip, vstack_ptr, frame_ptr}, ip->fiber_suspend.output_count);
        auto regs          = new_fiber->resume();
        process->cur_fiber = new_fiber;

        ip         = regs.ip;
        vstack_ptr = regs.vstack_ptr;
        frame_ptr  = regs.frame_ptr;

        ++ip;
        LAUF_VM_DISPATCH;
    }
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

LAUF_VM_EXECUTE(local_addr)
{
    auto allocation_idx = frame_ptr->first_local_alloc + ip->local_addr.value;

    --vstack_ptr;
    vstack_ptr[0].as_address.allocation = std::uint32_t(allocation_idx);
    vstack_ptr[0].as_address.offset     = 0;
    vstack_ptr[0].as_address.generation = frame_ptr->local_generation;

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(cc)
{
    switch (lauf_asm_inst_condition_code(ip->cc.value))
    {
    case LAUF_ASM_INST_CC_EQ:
        if (vstack_ptr[0].as_sint == 0)
            vstack_ptr[0].as_uint = 1;
        else
            vstack_ptr[0].as_uint = 0;
        break;
    case LAUF_ASM_INST_CC_NE:
        if (vstack_ptr[0].as_sint != 0)
            vstack_ptr[0].as_uint = 1;
        else
            vstack_ptr[0].as_uint = 0;
        break;
    case LAUF_ASM_INST_CC_LT:
        if (vstack_ptr[0].as_sint < 0)
            vstack_ptr[0].as_uint = 1;
        else
            vstack_ptr[0].as_uint = 0;
        break;
    case LAUF_ASM_INST_CC_LE:
        if (vstack_ptr[0].as_sint <= 0)
            vstack_ptr[0].as_uint = 1;
        else
            vstack_ptr[0].as_uint = 0;
        break;
    case LAUF_ASM_INST_CC_GT:
        if (vstack_ptr[0].as_sint > 0)
            vstack_ptr[0].as_uint = 1;
        else
            vstack_ptr[0].as_uint = 0;
        break;
    case LAUF_ASM_INST_CC_GE:
        if (vstack_ptr[0].as_sint >= 0)
            vstack_ptr[0].as_uint = 1;
        else
            vstack_ptr[0].as_uint = 0;
        break;
    }

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
LAUF_VM_EXECUTE(setup_local_alloc)
{
    // If necessary, grow the allocation array - this will then tail call back here.
    if (LAUF_UNLIKELY(process->memory.needs_to_grow(ip->setup_local_alloc.value)))
        LAUF_TAIL_CALL return grow_allocation_array(ip, vstack_ptr, frame_ptr, process);

    // Setup the necessary metadata.
    frame_ptr->first_local_alloc = process->memory.next_index();
    frame_ptr->local_generation  = process->memory.cur_generation();

    ++ip;
    LAUF_VM_DISPATCH;
}
LAUF_VM_EXECUTE(local_alloc)
{
    // The builder has taken care of ensuring alignment.
    assert(ip->local_alloc.alignment() == alignof(void*));
    assert(lauf::is_aligned(frame_ptr->next_frame(), alignof(void*)));

    auto memory = frame_ptr->next_frame();
    frame_ptr->next_offset += ip->local_alloc.size;

    process->memory.new_allocation_unchecked(
        lauf::make_local_alloc(memory, ip->local_alloc.size, frame_ptr->local_generation));

    ++ip;
    LAUF_VM_DISPATCH;
}
LAUF_VM_EXECUTE(local_alloc_aligned)
{
    // We need to ensure the starting address is aligned.
    auto memory = static_cast<unsigned char*>(frame_ptr->next_frame());
    memory += lauf::align_offset(memory, ip->local_alloc_aligned.alignment());
    // However, to increment the offset we need both alignment and size, as that was the offset
    // computation in the builder assumes.
    frame_ptr->next_offset += ip->local_alloc_aligned.alignment() + ip->local_alloc.size;

    process->memory.new_allocation_unchecked(
        lauf::make_local_alloc(memory, ip->local_alloc.size, frame_ptr->local_generation));

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(local_free)
{
    for (auto i = 0u; i != ip->local_free.value; ++i)
    {
        auto  index = frame_ptr->first_local_alloc + i;
        auto& alloc = process->memory[index];

        if (LAUF_UNLIKELY(alloc.split != lauf::allocation_split::unsplit))
            LAUF_DO_PANIC("cannot free split allocation");

        alloc.status = lauf::allocation_status::freed;
    }
    process->memory.remove_freed();

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(deref_const)
{
    auto address = vstack_ptr[0].as_address;

    auto alloc = process->memory.try_get(address);
    if (LAUF_UNLIKELY(alloc == nullptr))
        goto panic;

    {
        auto ptr = lauf::checked_offset(*alloc, address,
                                        {ip->deref_const.size, ip->deref_const.alignment()});
        if (LAUF_UNLIKELY(ptr == nullptr))
            goto panic;

        vstack_ptr[0].as_native_ptr = const_cast<void*>(ptr);
    }

    ++ip;
    LAUF_VM_DISPATCH;

panic:
    LAUF_DO_PANIC("invalid address");
}

LAUF_VM_EXECUTE(deref_mut)
{
    auto address = vstack_ptr[0].as_address;

    auto alloc = process->memory.try_get(address);
    if (LAUF_UNLIKELY(alloc == nullptr) || LAUF_UNLIKELY(lauf::is_const(alloc->source)))
        goto panic;

    {
        auto ptr = lauf::checked_offset(*alloc, address,
                                        {ip->deref_mut.size, ip->deref_mut.alignment()});
        if (LAUF_UNLIKELY(ptr == nullptr))
            goto panic;

        vstack_ptr[0].as_native_ptr = const_cast<void*>(ptr);
    }

    ++ip;
    LAUF_VM_DISPATCH;

panic:
    LAUF_DO_PANIC("invalid address");
}

LAUF_VM_EXECUTE(array_element)
{
    auto address = vstack_ptr[1].as_address;
    auto index   = vstack_ptr[0].as_sint;

    address.offset += lauf_sint(ip->array_element.value) * index;

    ++vstack_ptr;
    vstack_ptr[0].as_address = address;

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(aggregate_member)
{
    auto address = vstack_ptr[0].as_address;
    address.offset += ip->aggregate_member.value;
    vstack_ptr[0].as_address = address;

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(load_local_value)
{
    auto memory = reinterpret_cast<unsigned char*>(frame_ptr) + ip->load_local_value.value;

    --vstack_ptr;
    vstack_ptr[0] = *reinterpret_cast<lauf_runtime_value*>(memory);

    ++ip;
    LAUF_VM_DISPATCH;
}

LAUF_VM_EXECUTE(store_local_value)
{
    auto memory = reinterpret_cast<unsigned char*>(frame_ptr) + ip->store_local_value.value;

    *reinterpret_cast<lauf_runtime_value*>(memory) = vstack_ptr[0];
    ++vstack_ptr;

    ++ip;
    LAUF_VM_DISPATCH;
}

