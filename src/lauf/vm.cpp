// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/vm.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <lauf/asm/module.hpp>
#include <lauf/asm/program.hpp>
#include <lauf/lib/debug.hpp>
#include <lauf/runtime/builtin.h>
#include <lauf/runtime/process.h>
#include <lauf/runtime/stacktrace.h>
#include <lauf/vm_execute.hpp>

const lauf_vm_allocator lauf_vm_null_allocator
    = {nullptr, [](void*, size_t, size_t) -> void* { return nullptr; },
       [](void*, void*, size_t) {}};

const lauf_vm_allocator lauf_vm_malloc_allocator
    = {nullptr,
       [](void*, size_t size, size_t alignment) {
           return alignment > alignof(std::max_align_t) ? nullptr : std::calloc(size, 1);
       },
       [](void*, void* memory, size_t) { std::free(memory); }};

const lauf_vm_options lauf_default_vm_options = [] {
    lauf_vm_options result;

    result.initial_vstack_size_in_elements = 1024ull;
    result.max_vstack_size_in_elements     = 16 * 1024ull;

    result.initial_cstack_size_in_bytes = 16 * 1024ull;
    result.max_cstack_size_in_bytes     = 512 * 1024ull;

    result.step_limit = 0;

    result.panic_handler = [](lauf_runtime_process* process, const char* msg) {
        std::fprintf(stderr, "[lauf] panic: %s\n",
                     msg == nullptr ? "(invalid message pointer)" : msg);
        lauf::debug_print_cstack(process);
    };

    result.allocator = lauf_vm_malloc_allocator;

    return result;
}();

lauf_vm* lauf_create_vm(lauf_vm_options options)
{
    return lauf_vm::create(options);
}

void lauf_destroy_vm(lauf_vm* vm)
{
    lauf_vm::destroy(vm);
}

lauf_vm_panic_handler lauf_vm_set_panic_handler(lauf_vm* vm, lauf_vm_panic_handler h)
{
    auto old          = vm->panic_handler;
    vm->panic_handler = h;
    return old;
}

lauf_vm_allocator lauf_vm_set_allocator(lauf_vm* vm, lauf_vm_allocator a)
{
    auto old           = vm->heap_allocator;
    vm->heap_allocator = a;
    return old;
}

lauf_vm_allocator lauf_vm_get_allocator(lauf_vm* vm)
{
    return vm->heap_allocator;
}

namespace
{
void start_process(lauf_runtime_process* process, lauf_vm* vm, const lauf_asm_program* program)
{
    process->vm = vm;

    process->memory.init(vm->page_allocator, *vm, program->mod);
    process->program         = program;
    process->remaining_steps = vm->step_limit;

    process->cur_fiber = lauf::fiber::create(vm->page_allocator, process, vm->initial_vstack_size,
                                             vm->initial_cstack_size);
}

bool root_call(lauf_runtime_process* process, lauf_runtime_value* vstack_ptr,
               lauf_runtime_stack_frame* frame_ptr, const lauf_asm_function* fn)
{
    // Create the initial stack frame.
    auto trampoline_frame = process->cur_fiber->cstack.new_trampoline_frame(frame_ptr, fn);

    // Create the trampoline.
    lauf_asm_inst trampoline[2];
    trampoline[0].call.op = lauf::asm_op::call;
    // The current function of the stack frame is the one we want to call.
    trampoline[0].call.offset = 0;
    trampoline[1].exit.op     = lauf::asm_op::exit;

    // Execute the trampoline.
    return lauf::execute(trampoline, vstack_ptr, trampoline_frame, process);
}

void destroy_process(lauf_runtime_process* process)
{
    auto vm = process->vm;

    // Destroy all fibers.
    for (auto& fiber : process->fibers)
        fiber.destroy(vm->page_allocator, process);

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

    process->memory.clear(vm->page_allocator);
    process->fibers.clear(vm->page_allocator);
}
} // namespace

bool lauf_runtime_call(lauf_runtime_process* process, const lauf_asm_function* fn,
                       lauf_runtime_value* vstack_ptr)
{
    auto leaf = process->callstack_leaf_frame;

    auto result                   = root_call(process, vstack_ptr, leaf.prev, fn);
    process->callstack_leaf_frame = leaf;
    return result;
}

bool lauf_runtime_panic(lauf_runtime_process* process, const char* msg)
{
    // The process is nullptr during constant folding.
    if (process != nullptr)
        process->vm->panic_handler(process, msg);
    return false;
}

bool lauf_vm_execute(lauf_vm* vm, lauf_asm_program* program, const lauf_runtime_value* input,
                     lauf_runtime_value* output)
{
    auto fn  = lauf_asm_program_entry_function(program);
    auto sig = lauf_asm_function_signature(fn);

    lauf_runtime_process process;
    start_process(&process, vm, program);

    // Push input values onto the value stack.
    auto vstack_ptr = process.cur_fiber->vstack.base();
    for (auto i = 0u; i != sig.input_count; ++i)
    {
        --vstack_ptr;
        vstack_ptr[0] = input[i];
    }

    auto success = root_call(&process, vstack_ptr, nullptr, fn);
    if (success)
    {
        // Pop output values from the value stack.
        vstack_ptr = process.cur_fiber->vstack.base() - sig.output_count;
        for (auto i = 0u; i != sig.output_count; ++i)
        {
            output[sig.output_count - i - 1] = vstack_ptr[0];
            ++vstack_ptr;
        }
    }

    destroy_process(&process);
    return success;
}

bool lauf_vm_execute_oneshot(lauf_vm* vm, lauf_asm_program* program,
                             const lauf_runtime_value* input, lauf_runtime_value* output)
{
    auto result = lauf_vm_execute(vm, program, input, output);
    lauf_asm_destroy_program(program);
    return result;
}

