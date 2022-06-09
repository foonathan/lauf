// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/jit.h>

#include <cstdio> // TODO
#include <lauf/aarch64/emitter.hpp>
#include <lauf/impl/module.hpp>
#include <lauf/impl/vm.hpp>

namespace
{
class fake_stack
{
public:
    void reset()
    {
        _stack.clear();
        _next_free_register = 19;
    }

    std::uint8_t push()
    {
        assert(_next_free_register < 29); // TODO: bigger value stack
        _stack.push_back(_next_free_register);
        return _next_free_register++;
    }

    std::uint8_t pop()
    {
        auto reg = _stack.back();
        _stack.pop_back();
        --_next_free_register;
        return reg;
    }

private:
    // _stack.back() is the register that stores the value on top of the stack.
    std::vector<std::uint8_t> _stack;
    std::uint8_t              _next_free_register = 19;
};
} // namespace

struct lauf_jit_compiler_impl
{
    lauf::aarch64::emitter emitter;
    fake_stack             stack;
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
    compiler->stack.reset();

    //=== prologue ===//
    // Calling convention: x0 is parent ip, x1 is vstack_ptr, x3 is frame_ptr, x4 is process.
    // Register x19-x29 are used for the value stack.

    // Save callee saved registers x19-x29 and link register x30, in case we might need them.
    // TODO: only save them, when we need them.
    compiler->emitter.push_pair(19, 20);
    compiler->emitter.push_pair(21, 22);
    compiler->emitter.push_pair(23, 24);
    compiler->emitter.push_pair(25, 26);
    compiler->emitter.push_pair(27, 28);
    compiler->emitter.push_pair(29, 30);

    // Transfer the input values from the value stack.
    if (fn->input_count > 0)
    {
        for (auto i = 0u; i != fn->input_count; ++i)
            compiler->emitter.ldr_imm(compiler->stack.push(), 1, fn->input_count - i - 1);
        compiler->emitter.add_imm(1, 1, fn->input_count * 8);
    }

    auto emit_epilogue = [&] {
        // Transfer the output values to the value stack.
        if (fn->output_count > 0)
        {
            compiler->emitter.sub_imm(1, 1, fn->output_count * 8);
            for (auto i = 0u; i != fn->output_count; ++i)
                compiler->emitter.str_imm(compiler->stack.pop(), 1, i);
        }

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

            case lauf::bc_op::push: {
                auto reg = compiler->stack.push();
                auto lit = fn->mod->literal_data()[size_t(ip->push.literal_idx)];
                compiler->emitter.mov(reg, lit.as_uint);
                break;
            }
            case lauf::bc_op::push_zero: {
                auto reg = compiler->stack.push();
                compiler->emitter.mov(reg, std::uint16_t(0));
                break;
            }
            case lauf::bc_op::push_small_zext: {
                auto reg = compiler->stack.push();
                compiler->emitter.mov(reg, ip->push_small_zext.literal);
                break;
            }

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

