// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/vm.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <lauf/lib/debug.hpp>

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
    lauf_vm_options result{};

    result.initial_vstack_size_in_elements = 1024ull;
    result.max_vstack_size_in_elements     = 16 * 1024ull;

    result.initial_cstack_size_in_bytes = 16 * 1024ull;
    result.max_cstack_size_in_bytes     = 512 * 1024ull;

    result.step_limit = 0;

    result.panic_handler = {nullptr, [](void*, lauf_runtime_process* process, const char* msg) {
                                std::fprintf(stderr, "[lauf] panic: %s\n",
                                             msg == nullptr ? "(invalid message pointer)" : msg);
                                lauf::debug_print_all_cstacks(process);
                            }};

    result.allocator = lauf_vm_malloc_allocator;

    result.user_data = nullptr;

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

void* lauf_vm_set_user_data(lauf_vm* vm, void* user_data)
{
    auto old      = vm->user_data;
    vm->user_data = user_data;
    return old;
}

void* lauf_vm_get_user_data(lauf_vm* vm)
{
    return vm->user_data;
}

lauf_runtime_process* lauf_vm_start_process(lauf_vm* vm, const lauf_asm_program* program)
{
    auto fn = program->_entry;

    lauf_runtime_process::init(&vm->process, vm, program);
    vm->process.cur_fiber = lauf_runtime_create_fiber(&vm->process, fn);
    return &vm->process;
}

bool lauf_vm_execute(lauf_vm* vm, const lauf_asm_program* program, const lauf_runtime_value* input,
                     lauf_runtime_value* output)
{
    auto fn = program->_entry;

    lauf_runtime_process::init(&vm->process, vm, program);
    auto result = lauf_runtime_call(&vm->process, fn, input, output);
    lauf_runtime_process::cleanup(&vm->process);

    return result;
}

bool lauf_vm_execute_oneshot(lauf_vm* vm, lauf_asm_program program, const lauf_runtime_value* input,
                             lauf_runtime_value* output)
{
    auto result = lauf_vm_execute(vm, &program, input, output);
    if (program._extra_data != nullptr)
        lauf_asm_destroy_program(program);
    return result;
}

