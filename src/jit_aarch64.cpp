// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/jit.h>

#include <algorithm>
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
        for (auto r = 19; r <= 29; ++r)
            _pool.push_back(r);
    }

    std::uint8_t push()
    {
        assert(!_pool.empty()); // TODO
        auto r = _pool.back();
        _pool.pop_back();
        _stack.push_back(r);
        return r;
    }

    std::uint8_t pop()
    {
        auto reg = _stack.back();
        _stack.pop_back();

        auto contains = std::find(_stack.begin(), _stack.end(), reg) != _stack.end();
        if (!contains)
            // If the register isn't used already, push it back to the pool.
            _pool.push_back(reg);

        return reg;
    }
    void pop(std::size_t n)
    {
        for (auto i = 0u; i != n; ++i)
            pop();
    }

    void pick(std::size_t idx)
    {
        auto reg = _stack[_stack.size() - idx - 1];
        _stack.push_back(reg);
    }

    void roll(std::size_t idx)
    {
        auto iter = std::prev(_stack.end(), std::ptrdiff_t(idx) + 1);
        std::rotate(iter, iter + 1, _stack.end());
    }

private:
    // _stack.back() is the register that stores the value on top of the stack.
    std::vector<std::uint8_t> _stack;
    std::vector<std::uint8_t> _pool;
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
namespace
{

// Calling convention: x0 is parent ip, x1 is vstack_ptr, x2 is frame_ptr, x3 is process.
// Register x19-x29 are used for the value stack.
constexpr auto r_ip         = 0;
constexpr auto r_vstack_ptr = 1;
constexpr auto r_frame_ptr  = 2;
constexpr auto r_process    = 3;

void save_args(lauf_jit_compiler compiler)
{
    compiler->emitter.push_pair(r_ip, r_vstack_ptr);
    compiler->emitter.push_pair(r_frame_ptr, r_process);
}
void restore_args(lauf_jit_compiler compiler)
{
    compiler->emitter.pop_pair(r_frame_ptr, r_process);
    compiler->emitter.pop_pair(r_ip, r_vstack_ptr);
}

void load_from_value_stack(lauf_jit_compiler compiler, std::uint8_t count)
{
    if (count == 0)
        return;

    for (auto i = 0u; i != count; ++i)
        compiler->emitter.ldr_imm(compiler->stack.push(), r_vstack_ptr, count - i - 1);
    compiler->emitter.add_imm(r_vstack_ptr, r_vstack_ptr, count * 8);
}

void store_to_value_stack(lauf_jit_compiler compiler, std::uint8_t count)
{
    if (count == 0)
        return;

    compiler->emitter.sub_imm(r_vstack_ptr, r_vstack_ptr, count * 8);
    for (auto i = 0u; i != count; ++i)
        compiler->emitter.str_imm(compiler->stack.pop(), r_vstack_ptr, i);
}

template <typename Inst>
void call_builtin(lauf_jit_compiler compiler, lauf_builtin_function fn, Inst call)
{
    // Store the input values to the stack.
    store_to_value_stack(compiler, call.input);

    // As we're calling a function, we need to save registers.
    save_args(compiler);
    // Set r_ip to 0 so the builtin call returns and doesn't do a tail call.
    compiler->emitter.mov(r_ip, 0);
    // Call the builtin.
    compiler->emitter.call(fn);

    // Restore the previously saved arguments.
    restore_args(compiler);
    // Update the vstack_ptr to account for the stack change.
    compiler->emitter.add_imm(r_vstack_ptr, r_vstack_ptr, call.stack_change() * sizeof(lauf_value));

    // Load the output values into registers.
    load_from_value_stack(compiler, call.output);
}
} // namespace

lauf_builtin_function* lauf_jit_compile(lauf_jit_compiler compiler, lauf_function fn)
{
    compiler->emitter.clear();
    compiler->stack.reset();

    //=== prologue ===//
    // Save callee saved registers x19-x29 and link register x30, in case we might need them.
    // TODO: only save them, when we need them.
    compiler->emitter.push_pair(19, 20);
    compiler->emitter.push_pair(21, 22);
    compiler->emitter.push_pair(23, 24);
    compiler->emitter.push_pair(25, 26);
    compiler->emitter.push_pair(27, 28);
    compiler->emitter.push_pair(29, 30);

    // Transfer the input values from the value stack.
    load_from_value_stack(compiler, fn->input_count);

    auto emit_epilogue = [&] {
        // Transfer the output values to the value stack.
        store_to_value_stack(compiler, fn->output_count);

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

            case lauf::bc_op::call_builtin: {
                auto base_addr = reinterpret_cast<unsigned char*>(&lauf_builtin_dispatch);
                auto addr      = base_addr + ip->call_builtin.address * std::ptrdiff_t(16);
                auto fn        = reinterpret_cast<lauf_builtin_function*>(addr);
                call_builtin(compiler, fn, ip->call_builtin);
                break;
            }
            case lauf::bc_op::call_builtin_long: {
                auto addr
                    = fn->mod->literal_data()[size_t(ip->call_builtin_long.address)].as_native_ptr;
                auto fn = (lauf_builtin_function*)addr;
                call_builtin(compiler, fn, ip->call_builtin_long);
                break;
            }

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

            case lauf::bc_op::pop:
                compiler->stack.pop(ip->pop.literal);
                break;
            case lauf::bc_op::pick:
                compiler->stack.pick(ip->pick.literal);
                break;
            case lauf::bc_op::dup:
                compiler->stack.pick(0);
                break;
            case lauf::bc_op::roll:
                compiler->stack.roll(ip->roll.literal);
                break;
            case lauf::bc_op::swap:
                compiler->stack.roll(1);
                break;

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

