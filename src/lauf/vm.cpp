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

const lauf_vm_options lauf_default_vm_options
    = {512 * 1024ull, // max_cstack_size_in_bytes
       16 * 1024ull,  // initial_cstack_size_in-bytes
       16 * 1024ull,  // vstack_size_in_elements
       0,             // step_limit
       [](lauf_runtime_process* process, const char* msg) {
           std::fprintf(stderr, "[lauf] panic: %s\n",
                        msg == nullptr ? "(invalid message pointer)" : msg);
           lauf::debug_print_cstack(process);
       },
       lauf_vm_malloc_allocator};

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
    auto old      = vm->allocator;
    vm->allocator = a;
    return old;
}

lauf_vm_allocator lauf_vm_get_allocator(lauf_vm* vm)
{
    return vm->allocator;
}

namespace
{
lauf::allocation allocate_global(lauf::intrinsic_arena<lauf_vm>* arena, lauf_asm_global global)
{
    lauf::allocation result;

    if (global.memory != nullptr)
    {
        result.ptr = arena->memdup(global.memory, global.size, global.alignment);
    }
    else
    {
        result.ptr = arena->allocate(global.size, global.alignment);
        std::memset(result.ptr, 0, global.size);
    }

    // If bigger than 32bit, only the lower parts are addressable.
    result.size = std::uint32_t(global.size);

    result.source     = global.perms == lauf_asm_global::read_write
                            ? lauf::allocation_source::static_mut_memory
                            : lauf::allocation_source::static_const_memory;
    result.status     = lauf::allocation_status::allocated;
    result.gc         = lauf::gc_tracking::reachable_explicit;
    result.generation = 0;

    return result;
}

void start_process(lauf_runtime_process* process, lauf_vm* vm, const lauf_asm_program* program)
{
    process->vm                      = vm;
    process->vstack_end              = vm->vstack_end();
    process->remaining_cstack_chunks = vm->max_cstack_chunks;
    process->program                 = program;

    process->allocations.resize_uninitialized(*vm, program->mod->globals_count);
    for (auto global = program->mod->globals; global != nullptr; global = global->next)
        process->allocations[global->allocation_idx] = allocate_global(vm, *global);

    process->remaining_steps = vm->step_limit;
}

bool root_call(lauf_runtime_process* process, lauf_runtime_value* vstack_ptr, void* cstack_base,
               const lauf_asm_function* fn)
{
    // Create the initial stack frame.
    auto frame_ptr = ::new (cstack_base) auto(lauf_runtime_stack_frame::make_trampoline_frame(fn));
    assert(frame_ptr->is_trampoline_frame());

    // Create the trampoline.
    lauf_asm_inst trampoline[2];
    trampoline[0].call.op = lauf::asm_op::call;
    // The current function of the stack frame is the one we want to call.
    trampoline[0].call.offset = 0;
    trampoline[1].exit.op     = lauf::asm_op::exit;

    // Execute the trampoline.
    return lauf::execute(trampoline, vstack_ptr, frame_ptr, process);
}
} // namespace

bool lauf_runtime_call(lauf_runtime_process* process, const lauf_asm_function* fn,
                       lauf_runtime_value* vstack_ptr)
{
    auto leaf = process->callstack_leaf_frame;

    auto result                   = root_call(process, vstack_ptr, leaf.prev->next_frame(), fn);
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
    auto vstack_ptr = vm->vstack_base;
    for (auto i = 0u; i != sig.input_count; ++i)
    {
        --vstack_ptr;
        vstack_ptr[0] = input[i];
    }

    auto success = root_call(&process, vstack_ptr, vm->cstack->memory, fn);
    if (success)
    {
        // Pop output values from the value stack.
        vstack_ptr = vm->vstack_base - sig.output_count;
        for (auto i = 0u; i != sig.output_count; ++i)
        {
            output[sig.output_count - i - 1] = vstack_ptr[0];
            ++vstack_ptr;
        }
    }

    // Free allocated heap memory.
    for (auto alloc : process.allocations)
        if (alloc.source == lauf::allocation_source::heap_memory
            && alloc.status != lauf::allocation_status::freed)
        {
            if (alloc.split == lauf::allocation_split::unsplit)
                vm->allocator.free_alloc(vm->allocator.user_data, alloc.ptr, alloc.size);
            else if (alloc.split == lauf::allocation_split::split_first)
                // We don't know the full size.
                vm->allocator.free_alloc(vm->allocator.user_data, alloc.ptr, 0);
            else
                ; // We don't know the starting address of the allocation.
        }

    return success;
}

bool lauf_vm_execute_oneshot(lauf_vm* vm, lauf_asm_program* program,
                             const lauf_runtime_value* input, lauf_runtime_value* output)
{
    auto result = lauf_vm_execute(vm, program, input, output);
    lauf_asm_destroy_program(program);
    return result;
}

