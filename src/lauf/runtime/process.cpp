// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/runtime/process.hpp>

#include <lauf/vm.hpp>
#include <lauf/vm_execute.hpp>

lauf_runtime_fiber* lauf_runtime_fiber::create(lauf_runtime_process*    process,
                                               const lauf_asm_function* fn)
{
    auto vm = process->vm;

    // We first need to create the stack, as this also allocates the memory for the fiber itself.
    lauf::cstack stack;
    stack.init(vm->page_allocator, vm->initial_cstack_size);
    // We can then create a fiber in it and use it for the stack.
    auto fiber = ::new (stack.base()) lauf_runtime_fiber();

    auto addr = process->memory.new_allocation(vm->page_allocator, lauf::make_fiber_alloc(fiber));
    fiber->handle_allocation = addr.allocation;
    fiber->handle_generation = addr.generation;
    fiber->vstack.init(vm->page_allocator, vm->initial_vstack_size);
    fiber->cstack = stack;

    fiber->trampoline_frame.next_offset
        = sizeof(lauf_runtime_fiber) - offsetof(lauf_runtime_fiber, trampoline_frame);
    fiber->trampoline_frame.function = fn;

    fiber->suspension_point
        = {lauf::trampoline_code, fiber->vstack.base(), &fiber->trampoline_frame};
    fiber->expected_argument_count = fn->sig.input_count;

    // Add to linked list of fibers.
    // The order doesn't matter, so insert at front.
    fiber->next_fiber = process->fiber_list;
    if (process->fiber_list != nullptr)
        process->fiber_list->prev_fiber = fiber;
    process->fiber_list = fiber;

    return fiber;
}

void lauf_runtime_fiber::destroy(lauf_runtime_process* process, lauf_runtime_fiber* fiber)
{
    process->memory[fiber->handle_allocation].status = lauf::allocation_status::freed;

    if (fiber->prev_fiber == nullptr)
        process->fiber_list = fiber->next_fiber;
    else
        fiber->prev_fiber->next_fiber = fiber->next_fiber;
    if (fiber->next_fiber != nullptr)
        fiber->next_fiber->prev_fiber = fiber->prev_fiber;

    fiber->vstack.clear(process->vm->page_allocator);

    // At this point we're deallocating the memory for the fiber itself.
    fiber->cstack.clear(process->vm->page_allocator);
}

void lauf_runtime_process::init(lauf_runtime_process* process, lauf_vm* vm,
                                const lauf_asm_program* program)
{
    process->vm      = vm;
    process->program = *program;

    process->cur_fiber  = nullptr;
    process->fiber_list = nullptr;
    process->regs       = {};

    process->memory.init(vm, program);
    process->remaining_steps = vm->step_limit;
}

LAUF_NOINLINE void lauf_runtime_process::do_cleanup(lauf_runtime_process* process)
{
    auto vm = process->vm;

    // Free allocated heap and fiber memory that was leaked.
    for (auto alloc : process->memory)
    {
        if (alloc.status == lauf::allocation_status::freed)
            continue;

        if (alloc.source == lauf::allocation_source::heap_memory)
        {
            if (alloc.split == lauf::allocation_split::unsplit)
                vm->heap_allocator.free_alloc(vm->heap_allocator.user_data, alloc.ptr, alloc.size);
            else if (alloc.split == lauf::allocation_split::split_first)
                // We don't know the full size.
                vm->heap_allocator.free_alloc(vm->heap_allocator.user_data, alloc.ptr, 0);
            else
                ; // We don't know the starting address of the allocation.
        }
        else if (alloc.source == lauf::allocation_source::fiber_memory)
        {
            lauf_runtime_fiber::destroy(process, static_cast<lauf_runtime_fiber*>(alloc.ptr));
        }
    }

    process->memory.clear(vm);
}

lauf_vm* lauf_runtime_get_vm(lauf_runtime_process* process)
{
    return process->vm;
}

const lauf_asm_program* lauf_runtime_get_program(lauf_runtime_process* process)
{
    return &process->program;
}

lauf_runtime_fiber* lauf_runtime_get_current_fiber(lauf_runtime_process* process)
{
    assert(process->cur_fiber->status == lauf_runtime_fiber::running
           || process->cur_fiber->status == lauf_runtime_fiber::ready);
    return process->cur_fiber;
}

lauf_runtime_fiber* lauf_runtime_iterate_fibers(lauf_runtime_process* process)
{
    return process->fiber_list;
}

lauf_runtime_fiber* lauf_runtime_iterate_fibers_next(lauf_runtime_fiber* iter)
{
    return iter->next_fiber;
}

bool lauf_runtime_is_single_fibered(lauf_runtime_process* process)
{
    return process->fiber_list->next_fiber == nullptr;
}

lauf_runtime_address lauf_runtime_get_fiber_handle(const lauf_runtime_fiber* fiber)
{
    return fiber->handle();
}

lauf_runtime_fiber_status lauf_runtime_get_fiber_status(const lauf_runtime_fiber* fiber)
{
    switch (fiber->status)
    {
    case lauf_runtime_fiber::done:
        return LAUF_RUNTIME_FIBER_DONE;
    case lauf_runtime_fiber::ready:
        return LAUF_RUNTIME_FIBER_READY;
    case lauf_runtime_fiber::suspended:
        return LAUF_RUNTIME_FIBER_SUSPENDED;
    case lauf_runtime_fiber::running:
        return LAUF_RUNTIME_FIBER_RUNNING;
    }
}

lauf_runtime_fiber* lauf_runtime_get_fiber_parent(lauf_runtime_process* process,
                                                  lauf_runtime_fiber*   fiber)
{
    return fiber->has_parent() ? lauf::get_fiber(process, fiber->parent) : nullptr;
}

const lauf_runtime_value* lauf_runtime_get_vstack_ptr(lauf_runtime_process*     process,
                                                      const lauf_runtime_fiber* fiber)
{
    if (process->cur_fiber == fiber)
        return process->regs.vstack_ptr;
    else
        return fiber->suspension_point.vstack_ptr;
}

const lauf_runtime_value* lauf_runtime_get_vstack_base(const lauf_runtime_fiber* fiber)
{
    return fiber->vstack.base();
}

bool lauf_runtime_call(lauf_runtime_process* process, const lauf_asm_function* fn,
                       const lauf_runtime_value* input, lauf_runtime_value* output)
{
    // Save current processor state.
    auto regs                 = process->regs;
    auto cur_fiber            = process->cur_fiber;
    auto cur_allocation_index = process->memory.next_index();
    process->cur_fiber        = nullptr;

    // Create a new fiber.
    auto fiber = lauf_runtime_fiber::create(process, fn);

    // Resume the fiber at least once.
    auto success = false;
    if (LAUF_LIKELY(lauf_runtime_resume(process, fiber, input, fn->sig.input_count, output,
                                        fn->sig.output_count)))
    {
        success = true;

        // Then repeatedly until it is done.
        while (fiber->status != lauf_runtime_fiber::done)
        {
            if (!lauf_runtime_resume(process, fiber, nullptr, 0, output, fn->sig.output_count))
            {
                success = false;
                break;
            }
            fiber = process->cur_fiber;
        }
    }

    if (LAUF_UNLIKELY(!success))
    {
        // We paniced, so we manually need to mark all local memory allocations as freed before we
        // destroy the fiber. We don't need to worry about the split status, as the fiber is dead
        // anyway.
        for (auto alloc_idx = cur_allocation_index; alloc_idx != process->memory.next_index();
             ++alloc_idx)
        {
            auto& alloc = process->memory[alloc_idx];
            if (alloc.source == lauf::allocation_source::local_memory
                && alloc.status != lauf::allocation_status::freed)
            {
                alloc.status = lauf::allocation_status::freed;
                alloc.split  = lauf::allocation_split::unsplit;
                alloc.gc     = lauf::gc_tracking::unreachable;
            }
        }
    }

    // Destroy fiber and remove freed allocations.
    lauf_runtime_fiber::destroy(process, fiber);
    process->memory.remove_freed();

    // Restore processor state.
    process->regs      = regs;
    process->cur_fiber = cur_fiber;
    return success;
}

lauf_runtime_fiber* lauf_runtime_create_fiber(lauf_runtime_process*    process,
                                              const lauf_asm_function* fn)
{
    return lauf_runtime_fiber::create(process, fn);
}

bool lauf_runtime_resume(lauf_runtime_process* process, lauf_runtime_fiber* fiber,
                         const lauf_runtime_value* input, size_t input_count,
                         lauf_runtime_value* output, size_t output_count)
{
    assert(process->cur_fiber == nullptr
           || process->cur_fiber->status != lauf_runtime_fiber::running);
    if (LAUF_UNLIKELY(fiber->expected_argument_count != input_count))
        return lauf_runtime_panic(process, "mismatched signature for fiber resume");

    fiber->resume_by(nullptr);
    process->cur_fiber = fiber;

    // We can't call fiber->transfer_arguments() as the order is different.
    auto& vstack_ptr = fiber->suspension_point.vstack_ptr;
    for (auto i = 0u; i != input_count; ++i)
    {
        --vstack_ptr;
        vstack_ptr[0] = input[i];
    }

    auto success = lauf::execute(fiber->suspension_point.ip + 1, fiber->suspension_point.vstack_ptr,
                                 fiber->suspension_point.frame_ptr, process);
    if (LAUF_LIKELY(success))
    {
        // fiber could have changed, so reset back to the current fiber.
        fiber = process->cur_fiber;

        if (fiber->status == lauf_runtime_fiber::done)
        {
            // Copy the final arguments.
            auto actual_output_count = fiber->root_function()->sig.output_count;
            if (LAUF_UNLIKELY(actual_output_count != output_count))
                return lauf_runtime_panic(process, "mismatched signature for fiber resume");

            auto vstack_ptr = fiber->vstack.base() - actual_output_count;
            for (auto i = 0u; i != actual_output_count; ++i)
            {
                output[actual_output_count - i - 1] = vstack_ptr[0];
                ++vstack_ptr;
            }
        }
        else
        {
            // Copy the output values after resume.
            // If we don't have output values, we allow arbitrary output_count in call, otherwise,
            // it has to be strict. This makes it possible to just call it in a loop if the fiber
            // suspends without producing values.
            assert(fiber->status == lauf_runtime_fiber::suspended);
            auto actual_output_count = fiber->suspension_point.ip->fiber_suspend.input_count;
            if (actual_output_count > 0)
            {
                if (LAUF_UNLIKELY(actual_output_count != output_count))
                    return lauf_runtime_panic(process, "mismatched signature for fiber resume");

                auto& vstack_ptr = fiber->suspension_point.vstack_ptr;
                for (auto i = 0u; i != actual_output_count; ++i)
                {
                    output[actual_output_count - i - 1] = vstack_ptr[0];
                    ++vstack_ptr;
                }
            }
        }
    }
    return success;
}

void lauf_runtime_destroy_fiber(lauf_runtime_process* process, lauf_runtime_fiber* fiber)
{
    lauf_runtime_fiber::destroy(process, fiber);
}

bool lauf_runtime_panic(lauf_runtime_process* process, const char* msg)
{
    // The process is nullptr during constant folding.
    if (process != nullptr)
        process->vm->panic_handler(process, msg);
    return false;
}

void lauf_runtime_destroy_process(lauf_runtime_process* process)
{
    lauf_runtime_process::cleanup(process);
}

bool lauf_runtime_set_step_limit(lauf_runtime_process* process, size_t new_limit)
{
    auto vm_limit = process->vm->step_limit;
    if (vm_limit != 0 && new_limit > vm_limit)
        return false;

    process->remaining_steps = new_limit;
    assert(process->remaining_steps != 0);
    return true;
}

bool lauf_runtime_increment_step(lauf_runtime_process* process)
{
    // If the remaining steps are zero, we have no limit.
    if (process->remaining_steps > 0)
    {
        --process->remaining_steps;
        if (process->remaining_steps == 0)
            return false;

        // Note that if the panic recovers (via `lauf.test.assert_panic`), the process now
        // has an unlimited step limit.
    }

    return true;
}

