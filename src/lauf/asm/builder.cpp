// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/asm/builder.hpp>

#include <cstdio>
#include <cstdlib>

#include <lauf/asm/module.hpp>
#include <lauf/asm/type.h>
#include <lauf/runtime/builtin.h>

void lauf_asm_builder::error(const char* context, const char* msg)
{
    options.error_handler(fn->name, context, msg);
    errored = true;
}

const lauf_asm_build_options lauf_asm_default_build_options = {
    [](const char* fn_name, const char* context, const char* msg) {
        std::fprintf(stderr, "[lauf build error] %s() of '%s': %s\n", context, fn_name, msg);
        std::abort();
    },
};

lauf_asm_builder* lauf_asm_create_builder(lauf_asm_build_options options)
{
    return lauf_asm_builder::create(options);
}

void lauf_asm_destroy_builder(lauf_asm_builder* b)
{
    lauf_asm_builder::destroy(b);
}

void lauf_asm_build(lauf_asm_builder* b, lauf_asm_module* mod, lauf_asm_function* fn)
{
    b->reset(mod, fn);
}

bool lauf_asm_build_finish(lauf_asm_builder* b)
{
    constexpr auto context = LAUF_BUILD_ASSERT_CONTEXT;

    auto insts_count = [&] {
        auto result = std::size_t(0);
        for (auto& block : b->blocks)
        {
            block.offset = std::ptrdiff_t(result);

            result += block.insts.size();

            switch (block.terminator)
            {
            case lauf_asm_block::unterminated:
                b->error(context, "unterminated block");
                break;

            case lauf_asm_block::fallthrough:
                break;

            case lauf_asm_block::return_:
                ++result;
                if (b->local_allocation_count > 0)
                    ++result;
                break;

            case lauf_asm_block::jump:
            case lauf_asm_block::panic:
                ++result;
                break;

            case lauf_asm_block::branch2:
                result += 2;
                break;
            case lauf_asm_block::branch3:
                result += 3;
                break;
            }
        }

        if (result > UINT16_MAX)
            b->error(context, "too many instructions in function body");

        return result;
    }();

    auto insts = b->mod->allocate<lauf_asm_inst>(insts_count);

    auto ip = insts;
    for (auto& block : b->blocks)
    {
        ip = block.insts.copy_to(ip);

        auto cur_offset = ip - insts;
        switch (block.terminator)
        {
        case lauf_asm_block::unterminated:
        case lauf_asm_block::fallthrough:
            break;

        case lauf_asm_block::return_:
            if (b->local_allocation_count > 0)
                *ip++ = LAUF_BUILD_INST_VALUE(local_free, b->local_allocation_count);
            *ip++ = LAUF_BUILD_INST_NONE(return_);
            break;

        case lauf_asm_block::jump:
            if (block.next[0]->offset == cur_offset + 1)
                *ip++ = LAUF_BUILD_INST_NONE(nop);
            else
                *ip++ = LAUF_BUILD_INST_OFFSET(jump, block.next[0]->offset - cur_offset);
            break;

        case lauf_asm_block::branch2:
            *ip++ = LAUF_BUILD_INST_OFFSET(branch_false, block.next[1]->offset - cur_offset);
            ++cur_offset;

            if (block.next[0]->offset == cur_offset + 1)
                *ip++ = LAUF_BUILD_INST_NONE(nop);
            else
                *ip++ = LAUF_BUILD_INST_OFFSET(jump, block.next[0]->offset - cur_offset);
            break;

        case lauf_asm_block::branch3:
            *ip++ = LAUF_BUILD_INST_OFFSET(branch_eq, block.next[1]->offset - cur_offset);
            ++cur_offset;

            *ip++ = LAUF_BUILD_INST_OFFSET(branch_gt, block.next[2]->offset - cur_offset);
            ++cur_offset;

            if (block.next[0]->offset == cur_offset + 1)
                *ip++ = LAUF_BUILD_INST_NONE(nop);
            else
                *ip++ = LAUF_BUILD_INST_OFFSET(jump, block.next[0]->offset - cur_offset);
            break;

        case lauf_asm_block::panic:
            *ip++ = LAUF_BUILD_INST_NONE(panic);
            break;
        }
    }

    b->fn->insts       = insts;
    b->fn->insts_count = std::uint16_t(insts_count);

    b->fn->max_vstack_size = [&] {
        auto result = std::size_t(0);

        for (auto& block : b->blocks)
            if (block.vstack.max_size() > result)
                result = block.vstack.max_size();

        if (result > UINT16_MAX)
            b->error(context, "per-function vstack size limit exceeded");

        return std::uint16_t(result);
    }();

    b->fn->max_cstack_size = b->local_allocation_size;

    return !b->errored;
}

lauf_asm_local* lauf_asm_build_local(lauf_asm_builder* b, lauf_asm_layout layout)
{
    layout.size = lauf::round_to_multiple_of_alignment(layout.size, alignof(void*));

    if (layout.alignment <= alignof(void*))
    {
        // Ensure that the stack frame is always aligned to a pointer.
        // This means we can allocate without worrying about alignment.
        layout.alignment = alignof(void*);
        b->prologue->insts.push_back(*b, LAUF_BUILD_INST_LAYOUT(local_alloc, layout));
    }
    else
    {
        b->prologue->insts.push_back(*b, LAUF_BUILD_INST_LAYOUT(local_alloc_aligned, layout));
    }

    b->local_allocation_size += layout.size;
    if (layout.alignment > alignof(void*))
        b->local_allocation_size += layout.alignment;

    auto idx = b->local_allocation_count;
    ++b->local_allocation_count;
    return reinterpret_cast<lauf_asm_local*>(idx); // NOLINT
}

lauf_asm_block* lauf_asm_declare_block(lauf_asm_builder* b, lauf_asm_signature sig)
{
    if (b->blocks.size() == 1u)
        LAUF_BUILD_ASSERT(sig.input_count == b->fn->sig.input_count,
                          "requested entry block has different input count from function");

    return &b->blocks.emplace_back(*b, sig);
}

void lauf_asm_build_block(lauf_asm_builder* b, lauf_asm_block* block)
{
    LAUF_BUILD_ASSERT(block->terminator == lauf_asm_block::unterminated,
                      "cannot continue building a block that has been terminated already");

    b->cur = block;
}

void lauf_asm_inst_return(lauf_asm_builder* b)
{
    LAUF_BUILD_ASSERT_CUR;

    LAUF_BUILD_ASSERT(b->cur->vstack.finish(b->cur->sig.output_count),
                      "block's output count does not match vstack size on exit");
    LAUF_BUILD_ASSERT(b->cur->sig.output_count == b->fn->sig.output_count,
                      "requested exit block has different output count from function");

    b->cur->terminator = lauf_asm_block::return_;
    b->cur             = nullptr;
}

void lauf_asm_inst_jump(lauf_asm_builder* b, const lauf_asm_block* dest)
{
    LAUF_BUILD_ASSERT_CUR;

    LAUF_BUILD_ASSERT(b->cur->vstack.finish(b->cur->sig.output_count),
                      "block's output count does not match vstack size on exit");

    LAUF_BUILD_ASSERT(b->cur->sig.output_count == dest->sig.input_count,
                      "jump target's input count not compatible with current block's output count");
    b->cur->terminator = lauf_asm_block::jump;
    b->cur->next[0]    = dest;
    b->cur             = nullptr;
}

void lauf_asm_inst_branch2(lauf_asm_builder* b, const lauf_asm_block* if_true,
                           const lauf_asm_block* if_false)
{
    LAUF_BUILD_ASSERT_CUR;

    LAUF_BUILD_ASSERT(b->cur->vstack.pop(), "missing condition");
    LAUF_BUILD_ASSERT(b->cur->vstack.finish(b->cur->sig.output_count),
                      "block's output count does not match vstack size on exit");

    LAUF_BUILD_ASSERT(
        b->cur->sig.output_count == if_true->sig.input_count,
        "branch target's input count not compatible with current block's output count");
    LAUF_BUILD_ASSERT(
        b->cur->sig.output_count == if_false->sig.input_count,
        "branch target's input count not compatible with current block's output count");

    if (if_true == if_false)
    {
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_STACK_IDX(pop_top, 0));
        b->cur->terminator = lauf_asm_block::jump;
        b->cur->next[0]    = if_true;
    }
    else
    {
        b->cur->terminator = lauf_asm_block::branch2;
        b->cur->next[0]    = if_true;
        b->cur->next[1]    = if_false;
    }
    b->cur = nullptr;
}

void lauf_asm_inst_branch3(lauf_asm_builder* b, const lauf_asm_block* if_lt,
                           const lauf_asm_block* if_eq, const lauf_asm_block* if_false)
{
    LAUF_BUILD_ASSERT_CUR;

    LAUF_BUILD_ASSERT(b->cur->vstack.pop(), "missing condition");
    LAUF_BUILD_ASSERT(b->cur->vstack.finish(b->cur->sig.output_count),
                      "block's output count does not match vstack size on exit");

    LAUF_BUILD_ASSERT(
        b->cur->sig.output_count == if_lt->sig.input_count,
        "branch target's input count not compatible with current block's output count");
    LAUF_BUILD_ASSERT(
        b->cur->sig.output_count == if_eq->sig.input_count,
        "branch target's input count not compatible with current block's output count");
    LAUF_BUILD_ASSERT(
        b->cur->sig.output_count == if_false->sig.input_count,
        "branch target's input count not compatible with current block's output count");

    if (if_lt == if_eq && if_lt == if_false)
    {
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_STACK_IDX(pop_top, 0));
        b->cur->terminator = lauf_asm_block::jump;
        b->cur->next[0]    = if_lt;
    }
    else
    {
        b->cur->terminator = lauf_asm_block::branch3;
        b->cur->next[0]    = if_lt;
        b->cur->next[1]    = if_eq;
        b->cur->next[2]    = if_false;
    }
    b->cur = nullptr;
}

void lauf_asm_inst_panic(lauf_asm_builder* b)
{
    LAUF_BUILD_ASSERT_CUR;

    LAUF_BUILD_ASSERT(b->cur->vstack.pop(), "missing message");

    b->cur->terminator = lauf_asm_block::panic;
    b->cur             = nullptr;
}

void lauf_asm_inst_call(lauf_asm_builder* b, const lauf_asm_function* callee)
{
    LAUF_BUILD_ASSERT_CUR;

    LAUF_BUILD_ASSERT(b->cur->vstack.pop(callee->sig.input_count), "missing input values for call");

    auto offset = lauf::compress_pointer_offset(b->fn, callee);
    b->cur->insts.push_back(*b, LAUF_BUILD_INST_OFFSET(call, offset));

    b->cur->vstack.push(callee->sig.output_count);
}

void lauf_asm_inst_call_indirect(lauf_asm_builder* b, lauf_asm_signature sig)
{
    LAUF_BUILD_ASSERT_CUR;

    LAUF_BUILD_ASSERT(b->cur->vstack.pop(sig.input_count), "missing input values for call");
    LAUF_BUILD_ASSERT(b->cur->vstack.pop(), "missing function address");

    b->cur->insts.push_back(*b, LAUF_BUILD_INST_SIGNATURE(call_indirect, sig.input_count,
                                                          sig.output_count));

    b->cur->vstack.push(sig.output_count);
}

void lauf_asm_inst_call_builtin(lauf_asm_builder* b, lauf_runtime_builtin_function callee)
{
    LAUF_BUILD_ASSERT_CUR;

    LAUF_BUILD_ASSERT(b->cur->vstack.pop(callee.input_count), "missing input values for call");

    auto offset = lauf::compress_pointer_offset(&lauf_runtime_builtin_dispatch, callee.impl);
    if ((callee.flags & LAUF_RUNTIME_BUILTIN_NO_PROCESS) != 0)
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_OFFSET(call_builtin_no_process, offset));
    else
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_OFFSET(call_builtin, offset));

    b->cur->vstack.push(callee.output_count);
}

void lauf_asm_inst_sint(lauf_asm_builder* b, lauf_sint value)
{
    LAUF_BUILD_ASSERT_CUR;

    // We treat negative values as large positive values.
    lauf_asm_inst_uint(b, lauf_uint(value));
}

void lauf_asm_inst_uint(lauf_asm_builder* b, lauf_uint value)
{
    LAUF_BUILD_ASSERT_CUR;

    // For each bit pattern, the following is the minimal sequence of instructions to achieve it.
    if ((value & lauf_uint(0xFFFF'FFFF'FF00'0000)) == 0)
    {
        // 0x0000'0000'00xx'xxxx: push
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(push, std::uint32_t(value)));
    }
    else if ((value & lauf_uint(0xFFFF'0000'0000'0000)) == 0)
    {
        // 0x0000'yyyy'yyxx'xxxx: push + push2
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(push, std::uint32_t(value)));
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(push2, std::uint32_t(value >> 24)));
    }
    else if ((value & lauf_uint(0xFFFF'FFFF'FF00'0000)) == 0xFFFF'FFFF'FF00'0000)
    {
        // 0xFFFF'FFFF'FFxx'xxxx: pushn
        auto flipped = ~std::uint32_t(value) & 0xFF'FFFF;
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(pushn, flipped));
    }
    else
    {
        // 0xzzzz'yyyy'yyxx'xxxx: push + push2 + push3
        // Omit push2 if y = 0.
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(push, std::uint32_t(value)));
        if ((std::uint32_t(value >> 24) & 0xFF'FFFF) != 0)
            b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(push2, std::uint32_t(value >> 24)));
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(push3, std::uint32_t(value >> 48)));
    }

    b->cur->vstack.push();
}

void lauf_asm_inst_null(lauf_asm_builder* b)
{
    LAUF_BUILD_ASSERT_CUR;

    // NULL has all bits set.
    b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(pushn, 0));
    b->cur->vstack.push();
}

void lauf_asm_inst_global_addr(lauf_asm_builder* b, const lauf_asm_global* global)
{
    LAUF_BUILD_ASSERT_CUR;

    b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(global_addr, global->allocation_idx));
    b->cur->vstack.push();
}

void lauf_asm_inst_function_addr(lauf_asm_builder* b, const lauf_asm_function* function)
{
    LAUF_BUILD_ASSERT_CUR;

    auto offset = lauf::compress_pointer_offset(b->fn, function);
    b->cur->insts.push_back(*b, LAUF_BUILD_INST_OFFSET(function_addr, offset));
    b->cur->vstack.push();
}

void lauf_asm_inst_local_addr(lauf_asm_builder* b, const lauf_asm_local* local)
{
    LAUF_BUILD_ASSERT_CUR;

    auto index = reinterpret_cast<std::uint64_t>(local);
    b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(local_addr, std::uint32_t(index)));
    b->cur->vstack.push();
}

void lauf_asm_inst_pop(lauf_asm_builder* b, uint16_t stack_index)
{
    LAUF_BUILD_ASSERT_CUR;

    LAUF_BUILD_ASSERT(stack_index < b->cur->vstack.size(), "invalid stack index");

    if (stack_index == 0)
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_STACK_IDX(pop_top, stack_index));
    else
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_STACK_IDX(pop, stack_index));

    b->cur->vstack.pop();
}

void lauf_asm_inst_pick(lauf_asm_builder* b, uint16_t stack_index)
{
    LAUF_BUILD_ASSERT_CUR;

    LAUF_BUILD_ASSERT(stack_index < b->cur->vstack.size(), "invalid stack index");

    if (stack_index == 0)
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_STACK_IDX(dup, stack_index));
    else
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_STACK_IDX(pick, stack_index));

    b->cur->vstack.push();
}

void lauf_asm_inst_roll(lauf_asm_builder* b, uint16_t stack_index)
{
    LAUF_BUILD_ASSERT_CUR;

    LAUF_BUILD_ASSERT(stack_index < b->cur->vstack.size(), "invalid stack index");

    if (stack_index == 0)
        /* nothing needs to be done */;
    else if (stack_index == 1)
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_STACK_IDX(swap, stack_index));
    else
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_STACK_IDX(roll, stack_index));
}

void lauf_asm_inst_load_field(lauf_asm_builder* b, lauf_asm_type type, size_t field_index)
{
    LAUF_BUILD_ASSERT_CUR;

    b->cur->insts.push_back(*b, LAUF_BUILD_INST_LAYOUT(deref_const, type.layout));

    LAUF_BUILD_ASSERT(field_index < type.field_count, "invalid field index");
    lauf_asm_inst_uint(b, field_index);

    lauf_runtime_builtin_function builtin{};
    builtin.impl         = type.load_fn;
    builtin.input_count  = 2;
    builtin.output_count = 1;
    lauf_asm_inst_call_builtin(b, builtin);
}

void lauf_asm_inst_store_field(lauf_asm_builder* b, lauf_asm_type type, size_t field_index)
{
    LAUF_BUILD_ASSERT_CUR;

    b->cur->insts.push_back(*b, LAUF_BUILD_INST_LAYOUT(deref_mut, type.layout));

    LAUF_BUILD_ASSERT(field_index < type.field_count, "invalid field index");
    lauf_asm_inst_uint(b, field_index);

    lauf_runtime_builtin_function builtin{};
    builtin.impl         = type.store_fn;
    builtin.input_count  = 3;
    builtin.output_count = 0;
    lauf_asm_inst_call_builtin(b, builtin);
}

