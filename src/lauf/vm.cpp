// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/vm.hpp>

#include <cassert>
#include <cstdio>
#include <lauf/asm/module.hpp>
#include <lauf/asm/program.hpp>
#include <lauf/runtime/process.h>
#include <lauf/runtime/stacktrace.h>

const lauf_vm_options lauf_default_vm_options
    = {512 * 1024ull, 16 * 1024ull, [](lauf_runtime_process* process, const char* msg) {
           std::fprintf(stderr, "[lauf] panic: %s\n",
                        msg == nullptr ? "(invalid message pointer)" : msg);

           auto index = 0;
           for (auto st = lauf_runtime_get_stacktrace(process); st != nullptr;
                st      = lauf_runtime_stacktrace_parent(st))
           {
               auto fn   = lauf_runtime_stacktrace_function(st);
               auto addr = lauf_asm_get_instruction_index(fn, lauf_runtime_stacktrace_address(st));

               std::fprintf(stderr, " %4d. %s\n", index, lauf_asm_function_name(fn));
               std::fprintf(stderr, "       at <%04lx>\n", addr);
               ++index;
           }
       }};

lauf_vm* lauf_create_vm(lauf_vm_options options)
{
    return lauf_vm::create(options);
}

void lauf_destroy_vm(lauf_vm* vm)
{
    lauf_vm::destroy(vm);
}

bool lauf_vm_execute(lauf_vm* vm, lauf_asm_program* program, const lauf_runtime_value* input,
                     lauf_runtime_value* output)
{
    auto fn  = lauf_asm_entry_function(program);
    auto sig = lauf_asm_function_signature(fn);

    // Setup a new process.
    assert(vm->process.vm == nullptr);
    vm->process.vm = vm;

    vm->process.allocations.clear();
    for (auto global = program->mod->globals; global != nullptr; global = global->next)
        vm->process.allocations.push_back(lauf::allocation::allocate_global(vm, *global));

    // Create the initial stack frame.
    auto frame_ptr = ::new (vm->cstack_base) lauf::stack_frame{fn, nullptr, nullptr};

    // Push input values onto the value stack.
    auto vstack_ptr = vm->vstack_base;
    for (auto i = 0u; i != sig.input_count; ++i)
    {
        --vstack_ptr;
        vstack_ptr[0] = input[i];
    }

    // Create the trampoline.
    lauf::asm_inst trampoline[2];
    trampoline[0].call.op = lauf::asm_op::call;
    trampoline[0].call.offset
        = 0; // The current function of the stack frame is the one we want to call.
    trampoline[1].exit.op = lauf::asm_op::exit;

    // Execute the trampoline.
    if (!lauf::execute(trampoline, vstack_ptr, frame_ptr, &vm->process))
    {
        // It paniced.
        vm->process.vm = nullptr;
        return false;
    }

    // Pop output values from the value stack.
    vstack_ptr = vm->vstack_base - sig.output_count;
    for (auto i = 0u; i != sig.output_count; ++i)
    {
        output[sig.output_count - i - 1] = vstack_ptr[0];
        ++vstack_ptr;
    }

    return true;
}

bool lauf_vm_execute_oneshot(lauf_vm* vm, lauf_asm_program* program,
                             const lauf_runtime_value* input, lauf_runtime_value* output)
{
    auto result = lauf_vm_execute(vm, program, input, output);
    lauf_asm_destroy_program(program);
    return result;
}

