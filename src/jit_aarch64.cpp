// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/jit.h>

#include <algorithm>
#include <cstdio> // TODO
#include <lauf/aarch64/jit.hpp>
#include <lauf/impl/module.hpp>
#include <lauf/impl/vm.hpp>
#include <map>

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

template <typename Call, typename Imm>
void call_builtin(lauf_jit_compiler compiler, emitter::label lab_panic, const Call& call,
                  lauf_builtin_function fn, Imm ip)
{
    if (lauf_try_jit_int(compiler, fn))
    {
        compiler->emitter.cbz(register_::panic_result, lab_panic);
    }
    else
    {
        compiler->reg.push_inputs(compiler->emitter, r_vstack_ptr, call.input_count);

        save_args(compiler);
        {
            compiler->emitter.mov_imm(r_ip, ip);
            compiler->emitter.call(fn);
            compiler->emitter.mov(register_::panic_result, register_::result);
        }
        restore_args(compiler);
        compiler->emitter.cbz(register_::panic_result, lab_panic);

        compiler->reg.pop_outputs(call.output_count, call.stack_change());
    }
}
} // namespace

lauf_builtin_function* lauf_jit_compile(lauf_jit_compiler compiler, lauf_function fn)
{
    compiler->emitter.reset();
    compiler->reg.reset();

    const auto lab_success = compiler->emitter.declare_label();
    const auto lab_panic   = compiler->emitter.declare_label();

    //=== prologue ===//
    compiler->reg.save_restore_registers(true, compiler->emitter, fn->max_vstack_size);
    compiler->reg.enter_function(fn->input_count);

    //=== emit body ===//
    std::map<lauf_vm_instruction*, emitter::label> labels;
    auto                                           get_label_for = [&](lauf_vm_instruction* dest) {
        auto iter = labels.find(dest);
        if (iter != labels.end())
            return iter->second;

        return labels[dest] = compiler->emitter.declare_label();
    };

    for (auto ip = fn->bytecode(); ip != fn->bytecode() + fn->instruction_count; ++ip)
    {
        // TODO: make this less shitty
        auto iter = labels.find(ip);
        if (iter != labels.end())
            compiler->emitter.place_label(iter->second);
        else
            compiler->emitter.place_label(get_label_for(ip));

        switch (ip->tag.op)
        {
        case lauf::bc_op::nop:
        case lauf::bc_op::label:
            break;

        case lauf::bc_op::jump: {
            auto dest = get_label_for(ip + ip->jump.offset);
            compiler->reg.branch(compiler->emitter, r_vstack_ptr);
            compiler->emitter.b(dest);
            break;
        }
        case lauf::bc_op::jump_if:
        case lauf::bc_op::jump_ifz:
        case lauf::bc_op::jump_ifge: {
            auto dest = get_label_for(ip + ip->jump_if.offset + 1);
            auto reg  = compiler->reg.pop_as_register(compiler->emitter, r_vstack_ptr);
            compiler->reg.branch(compiler->emitter, r_vstack_ptr);
            switch (ip->jump_if.cc)
            {
            case lauf::condition_code::is_zero:
                compiler->emitter.cbz(reg, dest);
                break;
            case lauf::condition_code::is_nonzero:
                compiler->emitter.cbnz(reg, dest);
                break;

            case lauf::condition_code::cmp_lt:
                compiler->emitter.cmp_imm(reg, 0);
                compiler->emitter.b_cond(dest, lauf::aarch64::condition_code::lt);
                break;
            case lauf::condition_code::cmp_le:
                compiler->emitter.cmp_imm(reg, 0);
                compiler->emitter.b_cond(dest, lauf::aarch64::condition_code::le);
                break;
            case lauf::condition_code::cmp_gt:
                compiler->emitter.cmp_imm(reg, 0);
                compiler->emitter.b_cond(dest, lauf::aarch64::condition_code::gt);
                break;
            case lauf::condition_code::cmp_ge:
                compiler->emitter.cmp_imm(reg, 0);
                compiler->emitter.b_cond(dest, lauf::aarch64::condition_code::ge);
                break;
            }
            break;
        }

        case lauf::bc_op::return_:
        case lauf::bc_op::return_no_alloc:
            compiler->reg.exit_function(compiler->emitter, r_vstack_ptr, fn->output_count);
            compiler->emitter.b(lab_success);
            break;

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
            auto reg = compiler->reg.push_as_register();
            auto lit = fn->mod->literal_data()[size_t(ip->push.literal_idx)];
            compiler->emitter.mov_imm(reg, lit.as_uint);
            break;
        }
        case lauf::bc_op::push_zero: {
            auto reg = compiler->reg.push_as_register();
            compiler->emitter.mov_imm(reg, std::uint16_t(0));
            break;
        }
        case lauf::bc_op::push_small_zext: {
            auto reg = compiler->reg.push_as_register();
            compiler->emitter.mov_imm(reg, ip->push_small_zext.literal);
            break;
        }

        case lauf::bc_op::pop:
            compiler->reg.discard(ip->pop.literal);
            break;
        case lauf::bc_op::pick:
            compiler->reg.pick(compiler->emitter, r_vstack_ptr, ip->pick.literal);
            break;
        case lauf::bc_op::dup:
            compiler->reg.pick(compiler->emitter, r_vstack_ptr, 0);
            break;
        case lauf::bc_op::roll:
            compiler->reg.roll(compiler->emitter, r_vstack_ptr, ip->roll.literal);
            break;
        case lauf::bc_op::swap:
            compiler->reg.roll(compiler->emitter, r_vstack_ptr, 1);
            break;

        case lauf::bc_op::load_value:
            compiler->emitter.ldr_imm(compiler->reg.push_as_register(), r_frame_ptr,
                                      ip->load_value.literal);
            break;
        case lauf::bc_op::store_value: {
            auto reg = compiler->reg.pop_as_register(compiler->emitter, r_vstack_ptr);
            compiler->emitter.str_imm(reg, r_frame_ptr, ip->load_value.literal);
            break;
        }
        case lauf::bc_op::save_value: {
            auto reg = compiler->reg.top_as_register(compiler->emitter, r_vstack_ptr);
            compiler->emitter.str_imm(reg, r_frame_ptr, ip->load_value.literal);
            break;
        }

        default:
            assert(false); // unimplemented
            break;
        }
    }

    //=== lab_success ===//
    compiler->emitter.place_label(lab_success);

    compiler->reg.save_restore_registers(false, compiler->emitter, fn->max_vstack_size);
    compiler->emitter.tail_call(&lauf::jit_finish);

    //=== lab_panic ===//
    compiler->emitter.place_label(lab_panic);

    compiler->reg.save_restore_registers(false, compiler->emitter, fn->max_vstack_size);
    compiler->emitter.mov_imm(register_::result, std::uint64_t(0));
    compiler->emitter.ret();

    //=== finish ===//
    auto  jit_size = compiler->emitter.jit_size();
    auto& alloc    = fn->mod->exe_alloc;
    auto  jit_addr = alloc.allocate<alignof(std::uint32_t)>(jit_size);

    lauf::lock_executable_memory(alloc.memory());
    auto jit_fn_addr = compiler->emitter.finish(const_cast<void*>(jit_addr));
    lauf::unlock_executable_memory(alloc.memory());

    // TODO
    {
        auto file = std::fopen(fn->name, "w");
        std::fwrite(jit_addr, 1, jit_size, file);
        std::fclose(file);
    }

    return fn->jit_fn = reinterpret_cast<lauf_builtin_function*>(jit_fn_addr);
}

