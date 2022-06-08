// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/jit.h>

#include <cstdio> // TODO
#include <lauf/aarch64/emitter.hpp>
#include <lauf/impl/module.hpp>
#include <lauf/impl/vm.hpp>

struct lauf_jit_compiler_impl
{
    lauf::aarch64::emitter emitter;
};

//=== compiler functions ===//
lauf_jit_compiler lauf_jit_compiler_create(void)
{
    return new lauf_jit_compiler_impl{};
}

void lauf_jit_compiler_destroy(lauf_jit_compiler compiler)
{
    delete compiler;
}

lauf_jit_compiler lauf_vm_jit_compiler(lauf_vm vm)
{
    return vm->jit;
}

//=== compile ===//
lauf_builtin_function* lauf_jit_compile(lauf_jit_compiler compiler, lauf_function fn)
{
    compiler->emitter.clear();

    //=== prologue ===//
    // Save callee saved registers x19-x29 and link register x30, in case we might need them.
    // TODO: only save them, when we need them.
    compiler->emitter.push_pair(19, 20);
    compiler->emitter.push_pair(21, 22);
    compiler->emitter.push_pair(23, 24);
    compiler->emitter.push_pair(25, 26);
    compiler->emitter.push_pair(27, 28);
    compiler->emitter.push_pair(29, 30);

    auto emit_epilogue = [&] {
        // Restore the registers we've pushed above.
        compiler->emitter.pop_pair(29, 30);
        compiler->emitter.pop_pair(27, 28);
        compiler->emitter.pop_pair(25, 26);
        compiler->emitter.pop_pair(23, 24);
        compiler->emitter.pop_pair(21, 22);
        compiler->emitter.pop_pair(19, 20);
    };

    //=== emit body ===//
    auto emit = [&](lauf_vm_instruction* ip) {
        while (true)
        {
            switch (ip->tag.op)
            {
            case lauf::bc_op::nop:
                break;

            case lauf::bc_op::return_no_alloc:
                emit_epilogue();
                compiler->emitter.tail_call(&lauf_builtin_dispatch);
                return;

            case lauf::bc_op::push_zero:

            default:
                assert(false); // unimplemented
                break;
            }

            ++ip;
        }
    };
    emit(fn->bytecode());

    //=== finish ===//
    auto mod      = fn->mod;
    auto jit_size = compiler->emitter.size() * sizeof(uint32_t);
    if (mod->cur_jit_offset + jit_size > mod->jit_memory.size)
    {
        // TODO: adjust pointers, actual growth
        mod->jit_memory = lauf::resize_executable_memory(mod->jit_memory, 1);
    }

    lauf::lock_executable_memory(mod->jit_memory);
    auto jit_fn_addr = mod->jit_memory.ptr + mod->cur_jit_offset;
    std::memcpy(jit_fn_addr, compiler->emitter.data(), jit_size);
    mod->cur_jit_offset += compiler->emitter.size();
    lauf::unlock_executable_memory(mod->jit_memory);

    // TODO
    {
        auto file = std::fopen("test.bin", "w");
        std::fwrite(compiler->emitter.data(), sizeof(uint32_t), compiler->emitter.size(), file);
        std::fclose(file);
    }

    return fn->jit_fn = reinterpret_cast<lauf_builtin_function*>(jit_fn_addr);
}

