// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/jit.h>

#include <algorithm>
#include <cstdio> // TODO
#include <lauf/aarch64/assembler.hpp>
#include <lauf/aarch64/register.hpp>
#include <lauf/impl/module.hpp>
#include <lauf/impl/vm.hpp>
#include <lauf/ir/irgen.hpp>
#include <lauf/ir/register_allocator.hpp>
#include <map>

struct lauf_jit_compiler_impl
{
    lauf::memory_stack stack;
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
void emit_mov(lauf::aarch64::assembler& a, lauf::aarch64::register_nr xd, lauf_value value)
{
    auto bits = value.as_uint;
    a.movz(xd, lauf::aarch64::immediate(bits & 0xFF'FF));

    for (auto shift = std::uint8_t(16); true; shift += 16)
    {
        bits >>= 16;
        if (bits == 0)
            break;

        a.movk(xd, lauf::aarch64::immediate(bits & 0xFF'FF), lauf::aarch64::lsl{shift});
    }
}

// Doesn't need to be the exact size, just bigger.
constexpr auto trampoline_size          = 16;
constexpr auto trampoline_size_in_bytes = trampoline_size * sizeof(std::uint32_t);

lauf::aarch64::code compile_trampoline(lauf::stack_allocator& alloc, lauf_function fn)
{
    using namespace lauf::aarch64;
    assembler a(alloc);

    // Prepare vstack_ptr.
    auto reg_vstack = reg_temporary(0);
    if (fn->input_count > 1)
    {
        // tmp := vstack_ptr + (input_count - 1)
        a.add(reg_vstack, reg_argument(1), immediate((fn->input_count - 1) * sizeof(lauf_value)));
    }
    else
    {
        // tmp := vstack_ptr
        a.mov(reg_vstack, reg_argument(1));
    }

    // And save it, as well as link and frame.
    stack_push(a, reg_vstack, register_nr::link);
    stack_push(a, register_nr::frame);

    // Save return ip in IP0.
    a.mov(register_nr::ip0, reg_argument(0));
    // Save frame pointer in FP.
    a.mov(register_nr::frame, reg_argument(2));
    // Save process pointer in jit state register.
    a.mov(reg_jit_state, reg_argument(3));

    // Load argument registers from value stack.
    for (auto i = 0u; i != fn->input_count; ++i)
        a.ldr_post_imm(reg_argument(i), reg_vstack, immediate(sizeof(lauf_value)));

    // Call the actual function.
    a.bl(immediate(trampoline_size - a.cur_label_pos()));

    // Restore registers.
    stack_pop(a, register_nr::frame);
    stack_pop(a, reg_temporary(0), register_nr::link);
    // Push results back into the value stack.
    for (auto i = 0u; i != fn->output_count; ++i)
        a.str_post_imm(reg_argument(i), reg_vstack, immediate(-sizeof(lauf_value)));

    // And return back to the vm with exit code true/false depending on state register.
    // (reg_jit_state has been set to null after a panic.)
    a.cmp(reg_jit_state, immediate(0));
    a.cset(reg_argument(0), condition_code::ne);
    a.ret();

    return a.finish();
}

lauf::aarch64::code compile(lauf::stack_allocator& alloc, lauf_function fn,
                            const lauf::ir_function& irfn, const lauf::register_assignments& regs)
{
    using namespace lauf::aarch64;
    assembler a(alloc);

    auto                                    lab_entry  = a.declare_label();
    auto                                    lab_return = a.declare_label();
    std::optional<assembler::label>         lab_panic;
    lauf::temporary_array<assembler::label> labels(alloc, irfn.blocks().size());
    for (auto bb : irfn.blocks())
        labels.push_back(a.declare_label());

    //=== prologue ===//
    a.place_label(lab_entry);

    register_nr save_registers[32]  = {register_nr::frame, register_nr::link};
    auto        save_register_count = [&] {
        auto count = 2u; // frame + link

        // Add the persistent registers we're using.
        for (auto i = 0u; i <= regs.max_persistent_reg(); ++i)
            save_registers[count++] = reg_persistent(i);

        // If odd, also save a dummy register so we can always use the pair version.
        if (count % 2 == 1)
            save_registers[count++] = reg_temporary(0);

        return count;
    }();

    // Save all registers that need saving.
    for (auto i = 0u; i != save_register_count; i += 2)
        stack_push(a, save_registers[i], save_registers[i + 1]);

    // Setup stack frame.
    constexpr auto locals_offset = 3;
    stack_allocate(a, fn->local_stack_size + locals_offset * sizeof(lauf_value));
    a.str_imm(register_nr::frame, register_nr::stack,
              immediate(offsetof(lauf::stack_frame_base, prev) / sizeof(lauf_value)));
    a.str_imm(register_nr::ip0, register_nr::stack,
              immediate(offsetof(lauf::stack_frame_base, return_ip) / sizeof(lauf_value)));
    emit_mov(a, reg_temporary(0), [&] {
        lauf_value v;
        v.as_native_ptr = reinterpret_cast<void*>(fn);
        return v;
    }());
    a.str_imm(reg_temporary(0), register_nr::stack,
              immediate(offsetof(lauf::stack_frame_base, fn) / sizeof(lauf_value)));
    a.mov(register_nr::frame, register_nr::stack);

    //=== main body ===//
    auto set_argument_regs = [&](const lauf::ir_inst*& ip, unsigned arg_count) {
        for (auto i = 0u; i != arg_count; ++i)
        {
            // No -1 as ip points to the parent instruction.
            auto arg = ip[arg_count - i].argument;
            assert(arg.op == lauf::ir_op::argument);

            auto dest_reg = reg_argument(i);
            if (arg.is_constant)
                emit_mov(a, dest_reg, arg.constant);
            else if (auto cur_reg = reg_of(regs[arg.register_idx]); cur_reg != dest_reg)
                a.mov(dest_reg, cur_reg);
        }
        ip += arg_count;
    };
    auto set_result_regs = [&](const lauf::ir_inst*& ip, unsigned result_count) {
        for (auto i = 0u; i != result_count; ++i)
        {
            // +1 as ip points to the parent instruction.
            auto virt_reg = lauf::register_idx(irfn.index_of(ip[i + 1]));
            auto result   = ip[i + 1].call_result;
            assert(result.op == lauf::ir_op::call_result);

            if (result.uses > 0)
            {
                auto dest_reg = reg_of(regs[virt_reg]);
                auto cur_reg  = reg_argument(i);
                if (cur_reg != dest_reg)
                    a.mov(dest_reg, cur_reg);
            }
        }
        ip += result_count;
    };

    auto push_argument_regs
        = [&](const lauf::ir_inst*& ip, unsigned arg_count, unsigned vstack_offset) {
              for (auto i = 0u; i != arg_count; ++i)
              {
                  // No -1 as ip points to the parent instruction.
                  auto arg = ip[arg_count - i].argument;
                  assert(arg.op == lauf::ir_op::argument);

                  // First argument is at the bottom of the stack, so highest offset.
                  auto stack_offset = immediate(vstack_offset + (arg_count - i - 1));
                  if (arg.is_constant)
                  {
                      // We can use the argument register as a temporary; it's guaranteed to be
                      // unused.
                      auto tmp_reg = reg_argument(i);
                      emit_mov(a, tmp_reg, arg.constant);
                      a.str_imm(tmp_reg, register_nr::stack, stack_offset);
                  }
                  else
                  {
                      auto cur_reg = reg_of(regs[arg.register_idx]);
                      a.str_imm(cur_reg, register_nr::stack, stack_offset);
                  }
              }
              ip += arg_count;
          };

    auto pop_result_regs
        = [&](const lauf::ir_inst*& ip, unsigned result_count, unsigned vstack_offset) {
              for (auto i = 0u; i != result_count; ++i)
              {
                  // +1 as ip points to the last argument/call.
                  auto& result   = ip[i + 1];
                  auto  virt_reg = lauf::register_idx(irfn.index_of(result));

                  // First result is at the bottom of the stack, so highest offset.
                  auto stack_offset = immediate(vstack_offset + (result_count - i - 1));
                  auto dest_reg     = reg_of(regs[virt_reg]);
                  a.ldr_imm(dest_reg, register_nr::stack, stack_offset);
              }
              ip += result_count;
          };

    for (auto bb : irfn.blocks())
    {
        a.place_label(labels[std::size_t(bb)]);

        // When moving the stack pointer, we don't do it immediately, to avoid redundant add/sub
        // instruction. Instead, we accumulate changes and flush them only when the stack pointer
        // register is read (or at the end of the block).
        auto pending_sp_offset = 0;
        auto flush_sp          = [&] {
            if (pending_sp_offset > 0)
                stack_free(a, pending_sp_offset);
            else if (pending_sp_offset < 0)
                stack_allocate(a, -pending_sp_offset);
            pending_sp_offset = 0;
        };

        auto insts = irfn.block(bb);
        for (auto ip = insts.begin(); ip != insts.end(); ++ip)
        {
            auto& inst = *ip;
            if (inst.tag.uses == 0)
                continue;

            auto virt_reg = lauf::register_idx(irfn.index_of(inst));
            switch (inst.tag.op)
            {
            case lauf::ir_op::return_: {
                flush_sp();
                set_argument_regs(ip, inst.return_.argument_count);
                if (irfn.lexical_next_block(bb))
                    a.b(lab_return);
                break;
            }

            case lauf::ir_op::jump: {
                flush_sp();
                set_argument_regs(ip, inst.jump.argument_count);
                if (inst.jump.dest != irfn.lexical_next_block(bb))
                {
                    auto dest = labels[std::size_t(inst.jump.dest)];
                    a.b(dest);
                }
                break;
            }
            case lauf::ir_op::branch: {
                flush_sp();
                set_argument_regs(ip, inst.branch.argument_count);

                auto if_true  = labels[std::size_t(inst.branch.if_true)];
                auto cond_reg = reg_of(regs[inst.branch.reg]);
                switch (inst.branch.cc)
                {
                case lauf::condition_code::is_zero:
                    a.cbz(cond_reg, if_true);
                    break;
                case lauf::condition_code::is_nonzero:
                    a.cbnz(cond_reg, if_true);
                    break;

                case lauf::condition_code::cmp_lt:
                    a.cmp(cond_reg, immediate(0));
                    a.b(condition_code::lt, if_true);
                    break;
                case lauf::condition_code::cmp_le:
                    a.cmp(cond_reg, immediate(0));
                    a.b(condition_code::le, if_true);
                    break;
                case lauf::condition_code::cmp_gt:
                    a.cmp(cond_reg, immediate(0));
                    a.b(condition_code::gt, if_true);
                    break;
                case lauf::condition_code::cmp_ge:
                    a.cmp(cond_reg, immediate(0));
                    a.b(condition_code::ge, if_true);
                    break;
                }

                if (inst.branch.if_false != irfn.lexical_next_block(bb))
                {
                    auto if_false = labels[std::size_t(inst.branch.if_false)];
                    a.b(if_false);
                }
                break;
            }

            case lauf::ir_op::param: {
                auto cur_reg  = reg_argument(std::uint8_t(inst.param.index));
                auto dest_reg = reg_of(regs[virt_reg]);
                if (cur_reg != dest_reg)
                    a.mov(dest_reg, cur_reg);
                break;
            }
            case lauf::ir_op::const_:
                emit_mov(a, reg_of(regs[virt_reg]), inst.const_.value);
                break;

            case lauf::ir_op::call_builtin: {
                auto sig = inst.call_builtin.signature;

                // We need to use a value stack that can fit all inputs and all outputs.
                auto stack_size = std::max(sig.input_count, sig.output_count) * sizeof(lauf_value);
                pending_sp_offset -= int(stack_size);
                flush_sp();

                // Push the arguments. If we have more input values, they start at the bottom.
                // Otherwise, they start higher up.
                auto arg_offset
                    = sig.input_count >= sig.output_count ? 0u : sig.output_count - sig.input_count;
                push_argument_regs(ip, sig.input_count, arg_offset);

                // Set actual arguments.
                emit_mov(a, reg_argument(0), [&] {
                    lauf_value v;
                    v.as_native_ptr = fn->bytecode() + inst.call_builtin.bytecode_return_ip;
                    v.as_uint |= 1;
                    return v;
                }()); // ip = return_ip|1
                a.add(reg_argument(1), register_nr::stack,
                      immediate(arg_offset));               // vstack_ptr = SP + arg_offset
                a.mov(reg_argument(2), register_nr::frame); // frame_ptr = FP
                a.mov(reg_argument(3), reg_jit_state);      // process = jit_state

                // And call the function.
                emit_mov(a, reg_temporary(0), [&] {
                    lauf_value v;
                    v.as_native_ptr = reinterpret_cast<void*>(inst.call_builtin.fn);
                    return v;
                }());
                a.blr(reg_temporary(0));
                if (!lab_panic)
                    lab_panic = a.declare_label();
                a.cbz(reg_argument(0), *lab_panic);

                // Restore jit_state - it's kept in the third argument register.
                a.mov(reg_jit_state, reg_argument(3));

                // Store results. If we have more outputs, they start at the bottom.
                // Otherwise, they start higher up.
                auto result_offset
                    = sig.output_count >= sig.input_count ? 0u : sig.input_count - sig.output_count;
                pop_result_regs(ip, sig.output_count, result_offset);

                // Free the entire stack space.
                pending_sp_offset += int(stack_size);
                break;
            }
            case lauf::ir_op::call:
                assert(inst.call.fn == fn); // TODO: allow non-recursive call
                flush_sp();
                set_argument_regs(ip, inst.call.signature.input_count);
                emit_mov(a, register_nr::ip0, [&] {
                    lauf_value v;
                    v.as_native_ptr = fn->bytecode() + inst.call.bytecode_return_ip;
                    return v;
                }());
                a.bl(lab_entry);
                if (!lab_panic)
                    lab_panic = a.declare_label();
                a.cbz(reg_jit_state, *lab_panic);
                set_result_regs(ip, inst.call.signature.output_count);
                break;

            case lauf::ir_op::store_value: {
                auto value_reg = reg_of(regs[inst.store_value.register_idx]);
                a.str_imm(value_reg, register_nr::frame,
                          immediate(inst.store_value.local_addr + locals_offset));
                break;
            }
            case lauf::ir_op::load_value: {
                auto dest_reg = reg_of(regs[virt_reg]);
                a.ldr_imm(dest_reg, register_nr::frame,
                          immediate(inst.load_value.local_addr + locals_offset));
                break;
            }

            case lauf::ir_op::iadd:
            case lauf::ir_op::isub:
            case lauf::ir_op::scmp:
            case lauf::ir_op::ucmp: {
                auto dest_reg = reg_of(regs[virt_reg]);
                auto lhs      = reg_of(regs[inst.iadd.lhs]);
                auto rhs      = reg_of(regs[inst.iadd.rhs]);

                if (inst.tag.op == lauf::ir_op::iadd)
                    a.add(dest_reg, lhs, rhs);
                else if (inst.tag.op == lauf::ir_op::isub)
                    a.sub(dest_reg, lhs, rhs);
                else if (inst.tag.op == lauf::ir_op::scmp)
                {
                    a.cmp(lhs, rhs);
                    a.cset(lhs, condition_code::gt);
                    a.cset(rhs, condition_code::lt);
                    a.sub(dest_reg, lhs, rhs);
                }
                else if (inst.tag.op == lauf::ir_op::ucmp)
                {
                    a.cmp(lhs, rhs);
                    a.cset(lhs, condition_code::hi);
                    a.cset(rhs, condition_code::lo);
                    a.sub(dest_reg, lhs, rhs);
                }
                else
                    assert(false);
                break;
            }

            case lauf::ir_op::argument:
            case lauf::ir_op::call_result:
                assert(!"should be handled by parent instruction");
                break;
            }
        }
    }

    //=== epilogue ===//
    a.place_label(lab_return);

    // Free stack frame.
    a.mov(register_nr::stack, register_nr::frame);
    stack_free(a, fn->local_stack_size + locals_offset * sizeof(lauf_value));

    // Restore registers we've saved.
    for (auto i = 0u; i != save_register_count; i += 2)
        stack_pop(a, save_registers[save_register_count - i - 2],
                  save_registers[save_register_count - i - 1]);
    // And return from the function.
    a.ret();

    //=== panic propagation ===//
    if (lab_panic)
    {
        a.place_label(*lab_panic);
        a.movz(reg_jit_state, immediate(0));
        a.b(lab_return);
    }

    return a.finish();
}

} // namespace

bool lauf_jit_compile(lauf_jit_compiler compiler, lauf_function fn)
{
    compiler->stack.reset();
    lauf::stack_allocator alloc(compiler->stack);
    auto&                 exe_alloc = fn->mod->exe_alloc;

    auto irfn = lauf::irgen(alloc, fn);
    auto regs = lauf::register_allocation(alloc, lauf::aarch64::register_file, irfn);

    auto trampoline = compile_trampoline(alloc, fn);
    assert(trampoline.size_in_bytes <= trampoline_size_in_bytes);
    auto jitfn_trampoline = exe_alloc.allocate(trampoline.ptr, trampoline.size_in_bytes);
    exe_alloc.allocate(trampoline_size_in_bytes - trampoline.size_in_bytes);

    auto code  = compile(alloc, fn, irfn, regs);
    auto jitfn = exe_alloc.allocate(code.ptr, code.size_in_bytes);
    assert(exe_alloc.deref<char>(jitfn) - exe_alloc.deref<char>(jitfn_trampoline)
           == trampoline_size_in_bytes);

    // TODO
    {
        auto file = std::fopen(fn->name, "w");
        std::fwrite(exe_alloc.deref<void>(jitfn_trampoline), 1,
                    trampoline_size_in_bytes + code.size_in_bytes, file);
        std::fclose(file);
    }

    fn->jit_fn = jitfn_trampoline;
    return true;
}

