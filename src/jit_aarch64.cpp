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
            _pool.push_back(lauf::aarch64::register_(r));
    }

    auto push()
    {
        assert(!_pool.empty()); // TODO
        auto r = _pool.back();
        _pool.pop_back();
        _stack.push_back(r);
        return r;
    }

    auto pop()
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
    std::vector<lauf::aarch64::register_> _stack;
    std::vector<lauf::aarch64::register_> _pool;
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
using namespace lauf::aarch64;

// Calling convention: x0 is parent ip, x1 is vstack_ptr, x2 is frame_ptr, x3 is process.
// Register x19-x29 are used for the value stack.
constexpr auto r_ip         = register_(0);
constexpr auto r_vstack_ptr = register_(1);
constexpr auto r_frame_ptr  = register_(2);
constexpr auto r_process    = register_(3);

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

template <typename Call, typename Imm>
void call_builtin(lauf_jit_compiler compiler, emitter::label lab_panic, const Call& call,
                  lauf_builtin_function fn, Imm ip)
{
    store_to_value_stack(compiler, call.input_count);

    save_args(compiler);
    {
        compiler->emitter.mov_imm(r_ip, ip);
        compiler->emitter.call(fn);
        compiler->emitter.mov(register_::scratch1, register_::result);
    }
    restore_args(compiler);
    compiler->emitter.cbz(register_::scratch1, lab_panic);

    if (call.stack_change() > 0)
        compiler->emitter.add_imm(r_vstack_ptr, r_vstack_ptr,
                                  call.stack_change() * sizeof(lauf_value));
    else
        compiler->emitter.sub_imm(r_vstack_ptr, r_vstack_ptr,
                                  -call.stack_change() * sizeof(lauf_value));

    load_from_value_stack(compiler, call.output_count);
}
} // namespace

lauf_builtin_function* lauf_jit_compile(lauf_jit_compiler compiler, lauf_function fn)
{
    compiler->emitter.clear();
    compiler->stack.reset();

    const auto lab_success = compiler->emitter.declare_label();
    const auto lab_panic   = compiler->emitter.declare_label();

    //=== prologue ===//
    // Save callee saved registers x19-x29 and link register x30, in case we might need them.
    // TODO: only save them, when we need them.
    compiler->emitter.push_pair(register_(19), register_(20));
    compiler->emitter.push_pair(register_(21), register_(22));
    compiler->emitter.push_pair(register_(23), register_(24));
    compiler->emitter.push_pair(register_(25), register_(26));
    compiler->emitter.push_pair(register_(27), register_(28));
    compiler->emitter.push_pair(register_(29), register_(30));

    auto emit_epilogue = [&] {
        // Restore the registers we've pushed above.
        compiler->emitter.pop_pair(register_(29), register_(30));
        compiler->emitter.pop_pair(register_(27), register_(28));
        compiler->emitter.pop_pair(register_(25), register_(26));
        compiler->emitter.pop_pair(register_(23), register_(24));
        compiler->emitter.pop_pair(register_(21), register_(22));
        compiler->emitter.pop_pair(register_(19), register_(20));
    };

    // Transfer the input values from the value stack.
    load_from_value_stack(compiler, fn->input_count);

    //=== emit body ===//
    auto emit = [&](lauf_vm_instruction* ip) {
        while (true)
        {
            switch (ip->tag.op)
            {
            case lauf::bc_op::nop:
                break;

            case lauf::bc_op::return_no_alloc:
                compiler->emitter.b(lab_success);
                return;

            case lauf::bc_op::call: {
                auto callee = fn->mod->function_begin()[size_t(ip->call.function_idx)];
                call_builtin(compiler, lab_panic, *callee, &lauf::dispatch, ip);
                break;
            }
            case lauf::bc_op::call_builtin: {
                auto base_addr = reinterpret_cast<unsigned char*>(&lauf_builtin_finish);
                auto addr      = base_addr + ip->call_builtin.address * std::ptrdiff_t(16);
                auto fn        = reinterpret_cast<lauf_builtin_function*>(addr);

                auto dispatch_ip = reinterpret_cast<std::uintptr_t>(ip + 1) | 1;
                call_builtin(compiler, lab_panic, ip->call_builtin, fn, dispatch_ip);
                break;
            }
            case lauf::bc_op::call_builtin_long: {
                auto addr
                    = fn->mod->literal_data()[size_t(ip->call_builtin_long.address)].as_native_ptr;
                auto fn = (lauf_builtin_function*)addr;

                auto dispatch_ip = reinterpret_cast<std::uintptr_t>(ip + 1) | 1;
                call_builtin(compiler, lab_panic, ip->call_builtin_long, fn, dispatch_ip);
                break;
            }

            case lauf::bc_op::push: {
                auto reg = compiler->stack.push();
                auto lit = fn->mod->literal_data()[size_t(ip->push.literal_idx)];
                compiler->emitter.mov_imm(reg, lit.as_uint);
                break;
            }
            case lauf::bc_op::push_zero: {
                auto reg = compiler->stack.push();
                compiler->emitter.mov_imm(reg, std::uint16_t(0));
                break;
            }
            case lauf::bc_op::push_small_zext: {
                auto reg = compiler->stack.push();
                compiler->emitter.mov_imm(reg, ip->push_small_zext.literal);
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

    //=== lab_success ===//
    compiler->emitter.place_label(lab_success);

    // Transfer the output values to the value stack.
    store_to_value_stack(compiler, fn->output_count);

    emit_epilogue();
    compiler->emitter.tail_call(&lauf::jit_finish);

    //=== lab_panic ===//
    compiler->emitter.place_label(lab_panic);

    emit_epilogue();
    compiler->emitter.mov_imm(register_::result, std::uint64_t(0));
    compiler->emitter.ret();

    //=== finish ===//
    compiler->emitter.finish();

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

