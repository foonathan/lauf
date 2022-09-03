// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/runtime/process.hpp>

#include <lauf/asm/program.hpp>
#include <lauf/vm.hpp>
#include <lauf/vm_execute.hpp>

lauf::fiber* lauf::fiber::create(lauf_runtime_process* process, const lauf_asm_function* fn)
{
    auto vm = process->vm;

    lauf::cstack stack;
    stack.init(vm->page_allocator, vm->initial_cstack_size);

    auto fiber = ::new (stack.base()) lauf::fiber();

    fiber->handle = process->memory.new_allocation(vm->page_allocator, make_fiber_alloc(fiber));
    fiber->vstack.init(vm->page_allocator, vm->initial_vstack_size);
    fiber->cstack = stack;

    fiber->trampoline_frame.function = fn;
    fiber->trampoline_frame.next_offset
        = sizeof(lauf::fiber) - offsetof(lauf::fiber, trampoline_frame);

    fiber->next_fiber   = process->fiber_list;
    process->fiber_list = fiber;

    return fiber;
}

void lauf::fiber::destroy(lauf_runtime_process* process, fiber* fiber)
{
    process->memory[fiber->handle.allocation].status = lauf::allocation_status::freed;
    process->fiber_list                              = fiber->next_fiber;

    fiber->vstack.clear(process->vm->page_allocator);
    fiber->cstack.clear(process->vm->page_allocator);
}

namespace
{
constexpr lauf_asm_inst trampoline_code[2] = {
    [] {
        // We first want to call the function specified in the trampoline stack frame.
        lauf_asm_inst result;
        result.call.op     = lauf::asm_op::call;
        result.call.offset = 0;
        return result;
    }(),
    [] {
        // We then want to exit.
        lauf_asm_inst result;
        result.exit.op = lauf::asm_op::exit;
        return result;
    }(),
};
}

bool lauf::fiber::start(lauf_runtime_process* process, const lauf_runtime_value* input)
{
    assert(!is_running());
    auto vstack_ptr = vstack.base();

    // Copy input values.
    auto sig = lauf_asm_function_signature(root_function());
    for (auto i = 0u; i != sig.input_count; ++i)
    {
        --vstack_ptr;
        vstack_ptr[0] = input[i];
    }

    // Update the current fiber.
    resumer            = process->cur_fiber;
    process->cur_fiber = this;

    // Execute the trampoline.
    return lauf::execute(trampoline_code, vstack_ptr, &trampoline_frame, process);
}

void lauf::fiber::finish(lauf_runtime_value* output)
{
    assert(!is_running());

    auto sig        = lauf_asm_function_signature(root_function());
    auto vstack_ptr = vstack.base() - sig.output_count;
    for (auto i = 0u; i != sig.output_count; ++i)
    {
        output[sig.output_count - i - 1] = vstack_ptr[0];
        ++vstack_ptr;
    }
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
        lauf::fiber::destroy(process, fiber);

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

lauf_vm* lauf_runtime_get_vm(lauf_runtime_process* p)
{
    return p->vm;
}

const lauf_asm_program* lauf_runtime_get_program(lauf_runtime_process* p)
{
    return p->program;
}

const lauf_runtime_value* lauf_runtime_get_vstack_base(lauf_runtime_process* p)
{
    return p->cur_fiber->vstack.base();
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
    // Save current processor state.
    auto leaf      = process->callstack_leaf_frame;
    auto cur_fiber = process->cur_fiber;

    // Create, start, and destroy fiber.
    auto fiber  = lauf::fiber::create(process, fn);
    auto result = fiber->start(process, input);
    if (result)
        fiber->finish(output);
    lauf::fiber::destroy(process, fiber);

    // Restore processor state.
    process->callstack_leaf_frame = leaf;
    process->cur_fiber            = cur_fiber;
    return result;
}

bool lauf_runtime_set_step_limit(lauf_runtime_process* p, size_t new_limit)
{
    auto vm_limit = p->vm->step_limit;
    if (vm_limit != 0 && new_limit > vm_limit)
        return false;

    p->remaining_steps = new_limit;
    assert(p->remaining_steps != 0);
    return true;
}

bool lauf_runtime_increment_step(lauf_runtime_process* p)
{
    // If the remaining steps are zero, we have no limit.
    if (p->remaining_steps > 0)
    {
        --p->remaining_steps;
        if (p->remaining_steps == 0)
            return false;

        // Note that if the panic recovers (via `lauf.test.assert_panic`), the process now
        // has an unlimited step limit.
    }

    return true;
}

