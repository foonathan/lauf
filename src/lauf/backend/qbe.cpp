// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/backend/qbe.hpp>

#include <climits>
#include <lauf/asm/builder.h>
#include <lauf/asm/module.hpp>
#include <lauf/asm/type.h>
#include <lauf/runtime/builtin.h>

#include <lauf/lib/bits.h>
#include <lauf/lib/heap.h>
#include <lauf/lib/int.h>
#include <lauf/lib/memory.h>
#include <lauf/lib/platform.h>
#include <lauf/lib/test.h>

namespace
{
constexpr lauf_backend_qbe_extern_function default_externs[]
    = {{"lauf_heap_alloc", &lauf_lib_heap_alloc},
       {"lauf_heap_alloc_array", &lauf_lib_heap_alloc_array},
       {"lauf_heap_free", &lauf_lib_heap_free},
       {"lauf_heap_gc", &lauf_lib_heap_gc}};
}

const lauf_backend_qbe_options lauf_backend_default_qbe_options
    = {default_externs, sizeof(default_externs) / sizeof(default_externs[0])};

namespace
{
void codegen_global(lauf::qbe_writer&      writer, const lauf_backend_qbe_options&,
                    const lauf_asm_global* global)
{
    if (global->perms == lauf_asm_global::declaration)
        // Not a definition.
        return;

    writer.begin_data(lauf::qbe_data(global->allocation_idx), global->alignment);

    if (global->memory == nullptr)
    {
        // The global is zero initialized.
        writer.data_item(global->size);
    }
    else
    {
        for (auto i = 0u; i != global->size; ++i)
            writer.data_item(lauf::qbe_type::byte, global->memory[i]);
    }

    writer.end_data();
}

const char* extern_function_name(const lauf_backend_qbe_options& opts,
                                 lauf_runtime_builtin_impl*      impl)
{
    for (auto i = 0u; i != opts.extern_fns_count; ++i)
        if (opts.extern_fns[i].builtin->impl == impl)
            return opts.extern_fns[i].name;

    return nullptr;
}

void codegen_function(lauf::qbe_writer& writer, const lauf_backend_qbe_options& opts,
                      const lauf_asm_function* fn)
{
    if (fn->exported)
        writer.export_();

    if (fn->sig.output_count == 0)
        writer.begin_function(fn->name, lauf::qbe_void{});
    else if (fn->sig.output_count == 1)
        writer.begin_function(fn->name, lauf::qbe_type::value);
    else
        writer.begin_function(fn->name, writer.tuple(fn->sig.output_count));

    for (auto i = 0u; i != fn->sig.input_count; ++i)
        writer.param(lauf::qbe_type::value, i);

    writer.body();
    std::size_t vstack = fn->sig.input_count;

    auto block_id = [&](const lauf_asm_inst* ip) {
        while (ip->op() == lauf::asm_op::block)
            ++ip;
        return lauf::qbe_block(ip - fn->insts);
    };
    auto pop_reg = [&] {
        assert(vstack > 0);
        return lauf::qbe_reg(--vstack);
    };
    auto push_reg = [&] { return lauf::qbe_reg(vstack++); };

    auto next_block = [id = fn->inst_count]() mutable { return lauf::qbe_block(id++); };
    auto next_alloc = [id = 0]() mutable { return lauf::qbe_alloc(id++); };

    auto write_call
        = [&](lauf::qbe_value callee, std::uint8_t input_count, std::uint8_t output_count) {
              if (output_count == 0)
                  writer.begin_call(lauf::qbe_reg::tmp, lauf::qbe_void{}, callee);
              else if (output_count == 1)
                  writer.begin_call(lauf::qbe_reg::tmp, lauf::qbe_type::value, callee);
              else
                  writer.begin_call(lauf::qbe_reg::tmp, writer.tuple(output_count), callee);

              vstack -= input_count;
              for (auto i = 0u; i != input_count; ++i)
                  writer.argument(lauf::qbe_type::value, lauf::qbe_reg(vstack + i));

              writer.end_call();
              if (output_count == 1)
              {
                  writer.copy(push_reg(), lauf::qbe_type::value, lauf::qbe_reg::tmp);
              }
              else if (output_count > 1)
              {
                  for (auto i = 0u; i != output_count; ++i)
                  {
                      if (i > 0)
                          writer.binary_op(lauf::qbe_reg::tmp, lauf::qbe_type::value, "add",
                                           lauf::qbe_reg::tmp, std::uintmax_t(8));
                      writer.load(push_reg(), lauf::qbe_type::value, lauf::qbe_reg::tmp);
                  }
              }
          };

    auto dead_code = false;
    for (auto ip = fn->insts; ip != fn->insts + fn->inst_count; ++ip)
    {
        if (dead_code)
        {
            if (ip->op() != lauf::asm_op::block)
                continue;

            dead_code = false;
        }

        switch (ip->op())
        {
        case lauf::asm_op::nop:
            break;
        case lauf::asm_op::block:
            writer.block(block_id(ip));
            vstack = ip->block.input_count;
            break;

        case lauf::asm_op::return_:
        case lauf::asm_op::return_free:
            if (fn->sig.output_count == 0)
            {
                writer.ret();
            }
            else if (fn->sig.output_count == 1)
            {
                assert(vstack == 1);
                writer.ret(pop_reg());
                vstack = 0;
            }
            else
            {
                assert(vstack == fn->sig.output_count);
                writer.alloc8(lauf::qbe_alloc::return_, std::uintmax_t(fn->sig.output_count * 8));
                writer.copy(lauf::qbe_reg::tmp, lauf::qbe_type::value, lauf::qbe_alloc::return_);
                for (auto i = 0u; i != fn->sig.output_count; ++i)
                {
                    writer.store(lauf::qbe_type::value, lauf::qbe_reg(i), lauf::qbe_reg::tmp);
                    writer.binary_op(lauf::qbe_reg::tmp, lauf::qbe_type::value, "add",
                                     lauf::qbe_reg::tmp, std::uintmax_t(8));
                }
                writer.ret(lauf::qbe_alloc::return_);
            }
            break;

        case lauf::asm_op::jump:
            writer.jmp(block_id(ip + ip->jump.offset));
            break;

        case lauf::asm_op::branch_eq:
            writer.jnz(pop_reg(), block_id(ip + 1), block_id(ip + ip->branch_eq.offset));
            break;
        case lauf::asm_op::branch_ne:
            writer.jnz(pop_reg(), block_id(ip + ip->branch_ne.offset), block_id(ip + 1));
            break;

        case lauf::asm_op::branch_lt:
            writer.comparison(lauf::qbe_reg::tmp, lauf::qbe_cc::slt, lauf::qbe_type::value,
                              pop_reg(), std::uintmax_t(0));
            writer.jnz(lauf::qbe_reg::tmp, block_id(ip + ip->branch_lt.offset), block_id(ip + 1));
            break;
        case lauf::asm_op::branch_le:
            writer.comparison(lauf::qbe_reg::tmp, lauf::qbe_cc::sle, lauf::qbe_type::value,
                              pop_reg(), std::uintmax_t(0));
            writer.jnz(lauf::qbe_reg::tmp, block_id(ip + ip->branch_le.offset), block_id(ip + 1));
            break;
        case lauf::asm_op::branch_ge:
            writer.comparison(lauf::qbe_reg::tmp, lauf::qbe_cc::sge, lauf::qbe_type::value,
                              pop_reg(), std::uintmax_t(0));
            writer.jnz(lauf::qbe_reg::tmp, block_id(ip + ip->branch_ge.offset), block_id(ip + 1));
            break;
        case lauf::asm_op::branch_gt:
            writer.comparison(lauf::qbe_reg::tmp, lauf::qbe_cc::sgt, lauf::qbe_type::value,
                              pop_reg(), std::uintmax_t(0));
            writer.jnz(lauf::qbe_reg::tmp, block_id(ip + ip->branch_gt.offset), block_id(ip + 1));
            break;

        case lauf::asm_op::panic:
            writer.panic(pop_reg());
            dead_code = true;
            break;
        case lauf::asm_op::panic_if: {
            auto message   = pop_reg();
            auto condition = pop_reg();

            auto do_panic = next_block();
            auto no_panic = next_block();
            writer.jnz(condition, do_panic, no_panic);

            writer.block(do_panic);
            writer.panic(message);

            writer.block(no_panic);
            break;
        }

        case lauf::asm_op::call: {
            auto callee = lauf::uncompress_pointer_offset<lauf_asm_function>(fn, ip->call.offset);
            write_call(callee->name, callee->sig.input_count, callee->sig.output_count);
            break;
        }
        case lauf::asm_op::call_indirect:
            write_call(pop_reg(), ip->call_indirect.input_count, ip->call_indirect.output_count);
            break;

        case lauf::asm_op::call_builtin:
        case lauf::asm_op::call_builtin_no_regs: {
            assert(ip[1].op() == lauf::asm_op::call_builtin_sig);
            auto callee = lauf::uncompress_pointer_offset<lauf_runtime_builtin_impl> //
                (&lauf_runtime_builtin_dispatch, ip->call_builtin.offset);
            auto metadata = ip[1].call_builtin_sig;

            //=== VM directives ===//
            if ((metadata.flags & LAUF_RUNTIME_BUILTIN_VM_DIRECTIVE) != 0)
            {
                assert(metadata.output_count == 0);
                for (auto i = 0u; i != metadata.input_count; ++i)
                    pop_reg();
            }
            else if (auto extern_fn = extern_function_name(opts, callee))
            {
                write_call(extern_fn, metadata.input_count, metadata.output_count);
            }
            //=== type ===//
            else if (callee == lauf_asm_type_value.load_fn)
            {
                pop_reg(); // field index
                auto ptr = pop_reg();
                writer.load(push_reg(), lauf::qbe_type::value, ptr);
            }
            else if (callee == lauf_asm_type_value.store_fn)
            {
                pop_reg(); // field index
                auto ptr   = pop_reg();
                auto value = pop_reg();
                writer.store(lauf::qbe_type::value, value, ptr);
            }
            //=== bits ===//
            else if (callee == lauf_lib_bits_and.impl)
            {
                auto rhs  = pop_reg();
                auto lhs  = pop_reg();
                auto dest = push_reg();
                writer.binary_op(dest, lauf::qbe_type::value, "and", lhs, rhs);
            }
            else if (callee == lauf_lib_bits_or.impl)
            {
                auto rhs  = pop_reg();
                auto lhs  = pop_reg();
                auto dest = push_reg();
                writer.binary_op(dest, lauf::qbe_type::value, "or", lhs, rhs);
            }
            else if (callee == lauf_lib_bits_xor.impl)
            {
                auto rhs  = pop_reg();
                auto lhs  = pop_reg();
                auto dest = push_reg();
                writer.binary_op(dest, lauf::qbe_type::value, "xor", lhs, rhs);
            }
            else if (callee == lauf_lib_bits_shl.impl)
            {
                auto rhs  = pop_reg();
                auto lhs  = pop_reg();
                auto dest = push_reg();
                writer.binary_op(dest, lauf::qbe_type::value, "shl", lhs, rhs);
            }
            else if (callee == lauf_lib_bits_ushr.impl)
            {
                auto rhs  = pop_reg();
                auto lhs  = pop_reg();
                auto dest = push_reg();
                writer.binary_op(dest, lauf::qbe_type::value, "shr", lhs, rhs);
            }
            else if (callee == lauf_lib_bits_sshr.impl)
            {
                auto rhs  = pop_reg();
                auto lhs  = pop_reg();
                auto dest = push_reg();
                writer.binary_op(dest, lauf::qbe_type::value, "sar", lhs, rhs);
            }
            //=== int ===//
            else if (callee == lauf_lib_int_sadd(LAUF_LIB_INT_OVERFLOW_WRAP).impl
                     || callee == lauf_lib_int_uadd(LAUF_LIB_INT_OVERFLOW_WRAP).impl
                     || callee == lauf_lib_int_sadd(LAUF_LIB_INT_OVERFLOW_PANIC).impl
                     || callee == lauf_lib_int_uadd(LAUF_LIB_INT_OVERFLOW_PANIC).impl)
            {
                auto rhs  = pop_reg();
                auto lhs  = pop_reg();
                auto dest = push_reg();
                writer.binary_op(dest, lauf::qbe_type::value, "add", lhs, rhs);
            }
            else if (callee == lauf_lib_int_ssub(LAUF_LIB_INT_OVERFLOW_WRAP).impl
                     || callee == lauf_lib_int_usub(LAUF_LIB_INT_OVERFLOW_WRAP).impl
                     || callee == lauf_lib_int_ssub(LAUF_LIB_INT_OVERFLOW_PANIC).impl
                     || callee == lauf_lib_int_usub(LAUF_LIB_INT_OVERFLOW_PANIC).impl)
            {
                auto rhs  = pop_reg();
                auto lhs  = pop_reg();
                auto dest = push_reg();
                writer.binary_op(dest, lauf::qbe_type::value, "sub", lhs, rhs);
            }
            else if (callee == lauf_lib_int_smul(LAUF_LIB_INT_OVERFLOW_WRAP).impl
                     || callee == lauf_lib_int_umul(LAUF_LIB_INT_OVERFLOW_WRAP).impl
                     || callee == lauf_lib_int_smul(LAUF_LIB_INT_OVERFLOW_PANIC).impl
                     || callee == lauf_lib_int_umul(LAUF_LIB_INT_OVERFLOW_PANIC).impl)
            {
                auto rhs  = pop_reg();
                auto lhs  = pop_reg();
                auto dest = push_reg();
                writer.binary_op(dest, lauf::qbe_type::value, "mul", lhs, rhs);
            }
            else if (callee == lauf_lib_int_sdiv(LAUF_LIB_INT_OVERFLOW_WRAP).impl)
            {
                // Only do a division if we're not dividing MIN by -1.
                // Then just keep MIN as-is.
                auto rhs  = pop_reg();
                auto lhs  = pop_reg();
                auto dest = push_reg();
                assert(lhs == dest);

                writer.comparison(lauf::qbe_reg::tmp, lauf::qbe_cc::ieq, lauf::qbe_type::value, rhs,
                                  std::uintmax_t(-1));
                writer.comparison(lauf::qbe_reg::tmp2, lauf::qbe_cc::ieq, lauf::qbe_type::value,
                                  lhs, std::uintmax_t(INT64_MIN));
                writer.binary_op(lauf::qbe_reg::tmp, lauf::qbe_type::value, "and",
                                 lauf::qbe_reg::tmp, lauf::qbe_reg::tmp2);

                auto do_divide = next_block();
                auto end       = next_block();
                writer.jnz(lauf::qbe_reg::tmp, end, do_divide);
                writer.block(do_divide);
                writer.binary_op(dest, lauf::qbe_type::value, "div", lhs, rhs);
                writer.jmp(end);

                writer.block(end);
            }
            else if (callee == lauf_lib_int_sdiv(LAUF_LIB_INT_OVERFLOW_PANIC).impl)
            {
                // This version triggers an FPE exception on overflow, which is kinda like panic.
                auto rhs  = pop_reg();
                auto lhs  = pop_reg();
                auto dest = push_reg();
                writer.binary_op(dest, lauf::qbe_type::value, "div", lhs, rhs);
            }
            else if (callee == lauf_lib_int_udiv.impl)
            {
                auto rhs  = pop_reg();
                auto lhs  = pop_reg();
                auto dest = push_reg();
                writer.binary_op(dest, lauf::qbe_type::value, "udiv", lhs, rhs);
            }
            else if (callee == lauf_lib_int_srem.impl)
            {
                auto rhs  = pop_reg();
                auto lhs  = pop_reg();
                auto dest = push_reg();
                writer.binary_op(dest, lauf::qbe_type::value, "rem", lhs, rhs);
            }
            else if (callee == lauf_lib_int_urem.impl)
            {
                auto rhs  = pop_reg();
                auto lhs  = pop_reg();
                auto dest = push_reg();
                writer.binary_op(dest, lauf::qbe_type::value, "urem", lhs, rhs);
            }
            else if (callee == lauf_lib_int_scmp.impl)
            {
                auto rhs = pop_reg();
                auto lhs = pop_reg();

                writer.comparison(lauf::qbe_reg::tmp, lauf::qbe_cc::sgt, lauf::qbe_type::value, lhs,
                                  rhs);
                writer.comparison(lauf::qbe_reg::tmp2, lauf::qbe_cc::slt, lauf::qbe_type::value,
                                  lhs, rhs);
                writer.binary_op(push_reg(), lauf::qbe_type::value, "sub", lauf::qbe_reg::tmp,
                                 lauf::qbe_reg::tmp2);
            }
            else if (callee == lauf_lib_int_ucmp.impl)
            {
                auto rhs = pop_reg();
                auto lhs = pop_reg();

                writer.comparison(lauf::qbe_reg::tmp, lauf::qbe_cc::ugt, lauf::qbe_type::value, lhs,
                                  rhs);
                writer.comparison(lauf::qbe_reg::tmp2, lauf::qbe_cc::ult, lauf::qbe_type::value,
                                  lhs, rhs);
                writer.binary_op(push_reg(), lauf::qbe_type::value, "sub", lauf::qbe_reg::tmp,
                                 lauf::qbe_reg::tmp2);
            }
            else if (callee == lauf_lib_int_stou(LAUF_LIB_INT_OVERFLOW_WRAP).impl
                     || callee == lauf_lib_int_utos(LAUF_LIB_INT_OVERFLOW_WRAP).impl
                     || callee == lauf_lib_int_stou(LAUF_LIB_INT_OVERFLOW_PANIC).impl
                     || callee == lauf_lib_int_utos(LAUF_LIB_INT_OVERFLOW_PANIC).impl)
            { // NOLINT: repeated branch for clarity
              // No need to do anything.
            }
            else if (callee == lauf_lib_int_sabs(LAUF_LIB_INT_OVERFLOW_WRAP).impl
                     || callee == lauf_lib_int_sabs(LAUF_LIB_INT_OVERFLOW_PANIC).impl
                     || callee == lauf_lib_int_uabs.impl)
            {
                auto value = lauf::qbe_reg(vstack - 1);
                writer.comparison(lauf::qbe_reg::tmp, lauf::qbe_cc::slt, lauf::qbe_type::value,
                                  value, std::uintmax_t(0));

                auto flip = next_block();
                auto end  = next_block();
                writer.jnz(lauf::qbe_reg::tmp, flip, end);

                writer.block(flip);
                writer.binary_op(value, lauf::qbe_type::value, "mul", value, std::uintmax_t(-1));
                writer.jmp(end);

                writer.block(end);
            }
            else if (callee == lauf_lib_int_s8.load_fn || callee == lauf_lib_int_s16.load_fn
                     || callee == lauf_lib_int_s32.load_fn || callee == lauf_lib_int_u8.load_fn
                     || callee == lauf_lib_int_u16.load_fn || callee == lauf_lib_int_u32.load_fn)
            {
                pop_reg(); // field index
                auto ptr  = pop_reg();
                auto dest = push_reg();

                if (callee == lauf_lib_int_s8.load_fn)
                    writer.loadsb(dest, lauf::qbe_type::value, ptr);
                else if (callee == lauf_lib_int_u8.load_fn)
                    writer.loadub(dest, lauf::qbe_type::value, ptr);
                else if (callee == lauf_lib_int_s16.load_fn)
                    writer.loadsh(dest, lauf::qbe_type::value, ptr);
                else if (callee == lauf_lib_int_u16.load_fn)
                    writer.loaduh(dest, lauf::qbe_type::value, ptr);
                else if (callee == lauf_lib_int_s32.load_fn)
                    writer.loadsw(dest, lauf::qbe_type::value, ptr);
                else
                    writer.loaduw(dest, lauf::qbe_type::value, ptr);
            }
            else if (callee == lauf_lib_int_s8.store_fn || callee == lauf_lib_int_s16.store_fn
                     || callee == lauf_lib_int_s32.store_fn || callee == lauf_lib_int_u8.store_fn
                     || callee == lauf_lib_int_u16.store_fn || callee == lauf_lib_int_u32.store_fn)
            {
                pop_reg(); // field index
                auto ptr   = pop_reg();
                auto value = pop_reg();

                if (callee == lauf_lib_int_s8.store_fn || callee == lauf_lib_int_u8.store_fn)
                    writer.store(lauf::qbe_type::byte, value, ptr);
                else if (callee == lauf_lib_int_s16.store_fn || callee == lauf_lib_int_u16.store_fn)
                    writer.store(lauf::qbe_type::halfword, value, ptr);
                else
                    writer.store(lauf::qbe_type::word, value, ptr);
            }
            else if (callee == lauf_lib_int_s8_overflow.impl)
            {
                auto value = lauf::qbe_reg(vstack - 1);
                writer.comparison(lauf::qbe_reg::tmp, lauf::qbe_cc::sgt, lauf::qbe_type::value,
                                  value, std::uintmax_t(INT8_MAX));
                writer.comparison(lauf::qbe_reg::tmp2, lauf::qbe_cc::slt, lauf::qbe_type::value,
                                  value, std::uintmax_t(INT8_MIN));
                writer.binary_op(push_reg(), lauf::qbe_type::value, "or", lauf::qbe_reg::tmp,
                                 lauf::qbe_reg::tmp2);
            }
            else if (callee == lauf_lib_int_s16_overflow.impl)
            {
                auto value = lauf::qbe_reg(vstack - 1);
                writer.comparison(lauf::qbe_reg::tmp, lauf::qbe_cc::sgt, lauf::qbe_type::value,
                                  value, std::uintmax_t(INT16_MAX));
                writer.comparison(lauf::qbe_reg::tmp2, lauf::qbe_cc::slt, lauf::qbe_type::value,
                                  value, std::uintmax_t(INT16_MIN));
                writer.binary_op(push_reg(), lauf::qbe_type::value, "or", lauf::qbe_reg::tmp,
                                 lauf::qbe_reg::tmp2);
            }
            else if (callee == lauf_lib_int_s32_overflow.impl)
            {
                auto value = lauf::qbe_reg(vstack - 1);
                writer.comparison(lauf::qbe_reg::tmp, lauf::qbe_cc::sgt, lauf::qbe_type::value,
                                  value, std::uintmax_t(INT32_MAX));
                writer.comparison(lauf::qbe_reg::tmp2, lauf::qbe_cc::slt, lauf::qbe_type::value,
                                  value, std::uintmax_t(INT32_MIN));
                writer.binary_op(push_reg(), lauf::qbe_type::value, "or", lauf::qbe_reg::tmp,
                                 lauf::qbe_reg::tmp2);
            }
            else if (callee == lauf_lib_int_u8_overflow.impl)
            {
                auto value = lauf::qbe_reg(vstack - 1);
                writer.comparison(push_reg(), lauf::qbe_cc::sgt, lauf::qbe_type::value, value,
                                  std::uintmax_t(UINT8_MAX));
            }
            else if (callee == lauf_lib_int_u16_overflow.impl)
            {
                auto value = lauf::qbe_reg(vstack - 1);
                writer.comparison(push_reg(), lauf::qbe_cc::sgt, lauf::qbe_type::value, value,
                                  std::uintmax_t(UINT16_MAX));
            }
            else if (callee == lauf_lib_int_u32_overflow.impl)
            {
                auto value = lauf::qbe_reg(vstack - 1);
                writer.comparison(push_reg(), lauf::qbe_cc::sgt, lauf::qbe_type::value, value,
                                  std::uintmax_t(UINT32_MAX));
            }
            else if (callee == lauf_lib_int_s64_overflow.impl
                     || callee == lauf_lib_int_u64_overflow.impl)
            { // NOLINT: repeated branch for clarity
                writer.copy(push_reg(), lauf::qbe_type::value, std::uintmax_t(0));
            }
            //=== memory ===//
            else if (callee == lauf_lib_memory_addr_to_int.impl)
            {
                auto addr = pop_reg();
                writer.copy(lauf::qbe_reg::tmp, lauf::qbe_type::value, addr);

                auto provenance = push_reg();
                writer.copy(provenance, lauf::qbe_type::value, std::uintmax_t(0));

                auto integer = push_reg();
                writer.copy(integer, lauf::qbe_type::value, lauf::qbe_reg::tmp);
            }
            else if (callee == lauf_lib_memory_int_to_addr.impl)
            {
                auto integer = pop_reg();
                pop_reg();
                writer.copy(push_reg(), lauf::qbe_type::value, integer);
            }
            else if (callee == lauf_lib_memory_addr_add.impl)
            {
                auto offset = pop_reg();
                auto addr   = pop_reg();
                writer.binary_op(push_reg(), lauf::qbe_type::value, "add", addr, offset);
            }
            else if (callee == lauf_lib_memory_addr_sub.impl)
            {
                auto offset = pop_reg();
                auto addr   = pop_reg();
                writer.binary_op(push_reg(), lauf::qbe_type::value, "sub", addr, offset);
            }
            else if (callee == lauf_lib_memory_addr_distance.impl)
            {
                auto addr2 = pop_reg();
                auto addr1 = pop_reg();
                writer.binary_op(push_reg(), lauf::qbe_type::value, "sub", addr1, addr2);
            }
            //=== platform ===//
            else if (callee == lauf_lib_platform_vm.impl)
            {
                writer.copy(push_reg(), lauf::qbe_type::value, std::uintmax_t(0));
            }
            else if (callee == lauf_lib_platform_qbe.impl)
            {
                writer.copy(push_reg(), lauf::qbe_type::value, std::uintmax_t(1));
            }
            //=== test ===//
            else if (callee == lauf_lib_test_dynamic.impl || callee == lauf_lib_test_dynamic2.impl)
            {
                // Do nothing.
            }
            else if (callee == lauf_lib_test_unreachable.impl)
            {
                writer.panic(writer.literal("unrechable code reached"));
                dead_code = true;
            }
            else if (callee == lauf_lib_test_assert.impl)
            {
                auto if_true  = next_block();
                auto if_false = next_block();
                writer.jnz(pop_reg(), if_true, if_false);
                writer.block(if_false);
                writer.panic(writer.literal("assertion failure"));
                writer.block(if_true);
            }
            else if (callee == lauf_lib_test_assert_eq.impl)
            {
                auto rhs = pop_reg();
                auto lhs = pop_reg();

                writer.comparison(lauf::qbe_reg::tmp, lauf::qbe_cc::ieq, lauf::qbe_type::value, lhs,
                                  rhs);

                auto if_true  = next_block();
                auto if_false = next_block();
                writer.jnz(lauf::qbe_reg::tmp, if_true, if_false);

                writer.block(if_false);
                writer.panic(writer.literal("assertion failure"));

                writer.block(if_true);
            }
            else if (callee == lauf_lib_test_assert_panic.impl)
            {
                // We can't check whether something paniced, so we simply do nothing.
                pop_reg();
                pop_reg();
            }
            //=== error ===//
            else
            {
                writer.panic(writer.literal("unsupported - unknown builtin"));
                dead_code = true;
            }
            break;
        }
        case lauf::asm_op::call_builtin_sig:
            break;

        case lauf::asm_op::fiber_resume:
        case lauf::asm_op::fiber_transfer:
        case lauf::asm_op::fiber_suspend:
            writer.panic(writer.literal("unsupported - fiber"));
            dead_code = true;
            break;

        case lauf::asm_op::push: {
            auto value = std::uint64_t(ip->push.value);
            if (ip[1].op() == lauf::asm_op::push2)
            {
                value |= std::uint64_t(ip[1].push2.value) << 24;
                if (ip[2].op() == lauf::asm_op::push3)
                    value |= std::uint64_t(ip[2].push3.value) << 48;
            }
            else if (ip[1].op() == lauf::asm_op::push3)
            {
                value |= std::uint64_t(ip[1].push3.value) << 48;
            }
            writer.copy(push_reg(), lauf::qbe_type::value, value);
            break;
        }
        case lauf::asm_op::pushn: {
            auto value = ~std::uint64_t(ip->pushn.value);
            if (ip[1].op() == lauf::asm_op::push2)
            {
                value |= std::uint64_t(ip[1].push2.value) << 24;
                if (ip[2].op() == lauf::asm_op::push3)
                    value |= std::uint64_t(ip[2].push3.value) << 48;
            }
            else if (ip[1].op() == lauf::asm_op::push3)
            {
                value |= std::uint64_t(ip[1].push3.value) << 48;
            }
            writer.copy(push_reg(), lauf::qbe_type::value, value);
            break;
        }
        case lauf::asm_op::push2:
        case lauf::asm_op::push3:
            // Processed by push instruction above.
            break;

        case lauf::asm_op::global_addr:
            writer.copy(push_reg(), lauf::qbe_type::value, lauf::qbe_data(ip->global_addr.value));
            break;
        case lauf::asm_op::function_addr: {
            auto callee
                = lauf::uncompress_pointer_offset<lauf_asm_function>(fn, ip->function_addr.offset);
            writer.copy(push_reg(), lauf::qbe_type::value, callee->name);
            break;
        }
        case lauf::asm_op::local_addr:
            writer.copy(push_reg(), lauf::qbe_type::value, lauf::qbe_alloc(ip->local_addr.index));
            break;

        case lauf::asm_op::cc: {
            auto top  = pop_reg();
            auto dest = push_reg();
            switch (lauf_asm_inst_condition_code(ip->cc.value))
            {
            case LAUF_ASM_INST_CC_EQ:
                writer.comparison(dest, lauf::qbe_cc::ieq, lauf::qbe_type::value, top,
                                  std::uintmax_t(0));
                break;
            case LAUF_ASM_INST_CC_NE:
                writer.comparison(dest, lauf::qbe_cc::ine, lauf::qbe_type::value, top,
                                  std::uintmax_t(0));
                break;
            case LAUF_ASM_INST_CC_LT:
                writer.comparison(dest, lauf::qbe_cc::slt, lauf::qbe_type::value, top,
                                  std::uintmax_t(0));
                break;
            case LAUF_ASM_INST_CC_LE:
                writer.comparison(dest, lauf::qbe_cc::sle, lauf::qbe_type::value, top,
                                  std::uintmax_t(0));
                break;
            case LAUF_ASM_INST_CC_GT:
                writer.comparison(dest, lauf::qbe_cc::sgt, lauf::qbe_type::value, top,
                                  std::uintmax_t(0));
                break;
            case LAUF_ASM_INST_CC_GE:
                writer.comparison(dest, lauf::qbe_cc::sge, lauf::qbe_type::value, top,
                                  std::uintmax_t(0));
                break;
            }
            break;
        }

        case lauf::asm_op::pop:
        case lauf::asm_op::pop_top:
            for (auto i = vstack - 1 - ip->pop.idx; i != vstack - 1; ++i)
                writer.copy(lauf::qbe_reg(i), lauf::qbe_type::value, lauf::qbe_reg(i + 1));
            --vstack;
            break;

        case lauf::asm_op::pick:
        case lauf::asm_op::dup: {
            auto src = lauf::qbe_reg(vstack - 1 - ip->pick.idx);
            writer.copy(push_reg(), lauf::qbe_type::value, src);
            break;
        }

        case lauf::asm_op::roll:
        case lauf::asm_op::swap: {
            auto src = lauf::qbe_reg(vstack - 1 - ip->roll.idx);
            writer.copy(lauf::qbe_reg::tmp, lauf::qbe_type::value, src);
            for (auto i = vstack - 1 - ip->roll.idx; i != vstack - 1; ++i)
                writer.copy(lauf::qbe_reg(i), lauf::qbe_type::value, lauf::qbe_reg(i + 1));
            writer.copy(lauf::qbe_reg(vstack - 1), lauf::qbe_type::value, lauf::qbe_reg::tmp);
            break;
        }

        case lauf::asm_op::select: {
            auto index = pop_reg();
            auto end   = next_block();
            for (auto i = 0u; i <= ip->select.idx; ++i)
            {
                writer.comparison(lauf::qbe_reg::tmp, lauf::qbe_cc::ieq, lauf::qbe_type::value,
                                  index, std::uintmax_t(i));

                auto if_true  = next_block();
                auto if_false = next_block();
                writer.jnz(lauf::qbe_reg::tmp, if_true, if_false);
                writer.block(if_true);
                writer.copy(lauf::qbe_reg::tmp, lauf::qbe_type::value,
                            lauf::qbe_reg(vstack - 1 - i));
                writer.jmp(end);
                writer.block(if_false);
            }
            writer.panic(writer.literal("unreachable code reached"));
            writer.block(end);

            vstack -= ip->select.idx + 1;
            writer.copy(push_reg(), lauf::qbe_type::value, lauf::qbe_reg::tmp);
            break;
        }

        case lauf::asm_op::setup_local_alloc:
            // Nothing needs to be done.
            break;
        case lauf::asm_op::local_alloc:
        case lauf::asm_op::local_alloc_aligned:
            if (ip->local_alloc.alignment() <= 8)
                writer.alloc8(next_alloc(), std::uintmax_t(ip->local_alloc.size));
            else if (ip->local_alloc.alignment() <= 16)
                writer.alloc16(next_alloc(), std::uintmax_t(ip->local_alloc.size));
            else
            {
                // We allocate enough memory to allow for alignment padding.
                auto alloc = next_alloc();
                writer.alloc16(alloc,
                               std::uintmax_t(ip->local_alloc.size + ip->local_alloc.alignment()));

                // Compute the offset to align it properly.
                writer.binary_op(lauf::qbe_reg::tmp, lauf::qbe_type::value, "and", alloc,
                                 std::uintmax_t(ip->local_alloc.alignment() - 1));
                writer.binary_op(alloc, lauf::qbe_type::value, "add", alloc, lauf::qbe_reg::tmp);
            }
            break;
        case lauf::asm_op::local_storage:
            writer.alloc8(next_alloc(), std::uintmax_t(ip->local_storage.value));
            break;
        case lauf::asm_op::array_element: {
            auto index = pop_reg();
            auto ptr   = lauf::qbe_reg(vstack - 1);
            writer.binary_op(lauf::qbe_reg::tmp, lauf::qbe_type::value, "mul",
                             std::uintmax_t(ip->array_element.value), index);
            writer.binary_op(ptr, lauf::qbe_type::value, "add", ptr, lauf::qbe_reg::tmp);
            break;
        }
        case lauf::asm_op::aggregate_member: {
            auto ptr = lauf::qbe_reg(vstack - 1);
            writer.binary_op(ptr, lauf::qbe_type::value, "add", ptr,
                             std::uintmax_t(ip->aggregate_member.value));
            break;
        }
        case lauf::asm_op::deref_const:
        case lauf::asm_op::deref_mut:
            // Addresses are already pointers, so dereference does nothing.
            break;
        case lauf::asm_op::load_local_value:
            writer.load(push_reg(), lauf::qbe_type::value,
                        lauf::qbe_alloc(ip->load_local_value.index));
            break;
        case lauf::asm_op::store_local_value:
            writer.store(lauf::qbe_type::value, pop_reg(),
                         lauf::qbe_alloc(ip->store_local_value.index));
            break;
        case lauf::asm_op::load_global_value:
            writer.load(push_reg(), lauf::qbe_type::value,
                        lauf::qbe_data(ip->load_global_value.value));
            break;
        case lauf::asm_op::store_global_value:
            writer.store(lauf::qbe_type::value, pop_reg(),
                         lauf::qbe_data(ip->store_global_value.value));
            break;

        case lauf::asm_op::exit:
        case lauf::asm_op::count:
            assert(false && "unreachable");
            break;
        }
    }

    writer.end_function();
}
} // namespace

void lauf_backend_qbe(lauf_writer* _writer, lauf_backend_qbe_options options,
                      const lauf_asm_module* mod)
{
    lauf::qbe_writer writer;

    if (mod->globals != nullptr)
    {
        for (auto global = mod->globals; global != nullptr; global = global->next)
            codegen_global(writer, options, global);
    }

    if (mod->functions != nullptr)
    {
        for (auto fn = mod->functions; fn != nullptr; fn = fn->next)
            codegen_function(writer, options, fn);
    }

    std::move(writer).finish(_writer);
}

