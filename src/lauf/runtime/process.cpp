// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/runtime/process.hpp>

#include <lauf/asm/program.hpp>
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

lauf_runtime_process lauf_runtime_process::create(lauf_vm* vm, const lauf_asm_program* program)
{
    lauf_runtime_process result;
    result.vm      = vm;
    result.program = program;

    result.memory.init(vm, program->mod);
    result.remaining_steps = vm->step_limit;

    return result;
}

void lauf_runtime_process::destroy(lauf_runtime_process* process)
{
    auto vm = process->vm;

    // Destroy all fibers.
    while (auto fiber = process->fiber_list)
        lauf_runtime_fiber::destroy(process, fiber);

    // Free allocated heap memory.
    for (auto alloc : process->memory)
        if (alloc.source == lauf::allocation_source::heap_memory
            && alloc.status != lauf::allocation_status::freed)
        {
            if (alloc.split == lauf::allocation_split::unsplit)
                vm->heap_allocator.free_alloc(vm->heap_allocator.user_data, alloc.ptr, alloc.size);
            else if (alloc.split == lauf::allocation_split::split_first)
                // We don't know the full size.
                vm->heap_allocator.free_alloc(vm->heap_allocator.user_data, alloc.ptr, 0);
            else
                ; // We don't know the starting address of the allocation.
        }

    process->memory.clear(vm);
}

lauf_vm* lauf_runtime_get_vm(lauf_runtime_process* process)
{
    return process->vm;
}

const lauf_asm_program* lauf_runtime_get_program(lauf_runtime_process* process)
{
    return process->program;
}

lauf_runtime_fiber* lauf_runtime_get_current_fiber(lauf_runtime_process* process)
{
    assert(process->cur_fiber == nullptr
           || process->cur_fiber->state == lauf_runtime_fiber::running);
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

lauf_runtime_address lauf_runtime_get_fiber_handle(const lauf_runtime_fiber* fiber)
{
    return fiber->handle();
}

lauf_runtime_fiber_state lauf_runtime_get_fiber_state(const lauf_runtime_fiber* fiber)
{
    switch (fiber->state)
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

const lauf_runtime_value* lauf_runtime_get_vstack_base(const lauf_runtime_fiber* fiber)
{
    return fiber->vstack.base();
}

bool lauf_runtime_panic(lauf_runtime_process* process, const char* msg)
{
    // The process is nullptr during constant folding.
    if (process != nullptr)
        process->vm->panic_handler(process, msg);
    return false;
}

bool lauf_runtime_call(lauf_runtime_process* process, const lauf_asm_function* fn,
                       const lauf_runtime_value* input, lauf_runtime_value* output)
{
    auto sig = lauf_asm_function_signature(fn);

    // Save current processor state.
    auto regs          = process->regs;
    auto cur_fiber     = process->cur_fiber;
    process->cur_fiber = nullptr;

    // Create a new fiber.
    auto fiber = lauf_runtime_fiber::create(process, fn);

    // We need to manually transfer arguments as the order is different.
    auto& vstack_ptr = fiber->suspension_point.vstack_ptr;
    for (auto i = 0u; i != sig.input_count; ++i)
    {
        --vstack_ptr;
        vstack_ptr[0] = input[i];
    }

    // Execute the fiber until it is done.
    auto success = true;
    while (fiber->state != lauf_runtime_fiber::done)
    {
        if (!lauf_runtime_resume(process, fiber))
        {
            success = false;
            break;
        }

        fiber = process->cur_fiber;
    }
    if (success)
    {
        // The fiber could have changed, so we need to check that the output count still matches.
        if (fiber->root_function()->sig.output_count != sig.output_count)
            return lauf_runtime_panic(process, "invalid output count in call");

        // Copy the output values over.
        vstack_ptr = fiber->vstack.base() - sig.output_count;
        for (auto i = 0u; i != sig.output_count; ++i)
        {
            output[sig.output_count - i - 1] = vstack_ptr[0];
            ++vstack_ptr;
        }
    }

    lauf_runtime_fiber::destroy(process, fiber);

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

bool lauf_runtime_resume(lauf_runtime_process* process, lauf_runtime_fiber* fiber)
{
    assert(process->cur_fiber == nullptr);

    fiber->resume_by(nullptr);
    process->cur_fiber = fiber;

    return lauf::execute(fiber->suspension_point.ip + 1, fiber->suspension_point.vstack_ptr,
                         fiber->suspension_point.frame_ptr, process);
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

