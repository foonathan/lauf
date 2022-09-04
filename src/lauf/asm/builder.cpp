// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/asm/builder.hpp>

#include <cstdio>
#include <cstdlib>

#include <lauf/runtime/builtin.h>
#include <lauf/runtime/stack.hpp>

void lauf_asm_builder::error(const char* context, const char* msg)
{
    options.error_handler(fn->name, context, msg);
    errored = true;
}

namespace
{
void add_pop_top_n(lauf_asm_builder* b, std::size_t count)
{
    while (count > 0)
    {
        // The block can be come empty if we attempt to pop an argument.
        // In that case we treat it as any instruction that we couldn't remove anyway.
        auto op = b->cur->insts.empty() ? lauf::asm_op::call : b->cur->insts.back().op();
        switch (op)
        {
        case lauf::asm_op::count:
        case lauf::asm_op::nop:
        case lauf::asm_op::return_:
        case lauf::asm_op::jump:
        case lauf::asm_op::branch_false:
        case lauf::asm_op::branch_eq:
        case lauf::asm_op::branch_gt:
        case lauf::asm_op::panic:
        case lauf::asm_op::exit:
        case lauf::asm_op::setup_local_alloc:
        case lauf::asm_op::local_alloc:
        case lauf::asm_op::local_alloc_aligned:
        case lauf::asm_op::local_free:
            assert(false && "not added at this point");
            break;

        case lauf::asm_op::push:
        case lauf::asm_op::pushn:
        case lauf::asm_op::global_addr:
        case lauf::asm_op::function_addr:
        case lauf::asm_op::local_addr:
        case lauf::asm_op::pick:
        case lauf::asm_op::dup:
        case lauf::asm_op::load_local_value:
            // Signature 0 => 1, actually removed something.
            b->cur->insts.pop_back();
            --count;
            break;

        case lauf::asm_op::push2:
        case lauf::asm_op::push3:
        case lauf::asm_op::deref_const:
        case lauf::asm_op::deref_mut:
        case lauf::asm_op::aggregate_member:
        case lauf::asm_op::cc:
            // Signature 1 => 1, remove as well.
            b->cur->insts.pop_back();
            break;

        case lauf::asm_op::array_element:
            // Signature 2 => 1, we can remove it, but need to pop one more after we did
            // that.
            b->cur->insts.pop_back();
            ++count;
            break;

        // Instructions that we can't remove due to side-effects.
        case lauf::asm_op::call:
        case lauf::asm_op::call_indirect:
        case lauf::asm_op::call_builtin:
        case lauf::asm_op::call_builtin_no_frame:
        case lauf::asm_op::fiber_create:
        case lauf::asm_op::fiber_resume:
        case lauf::asm_op::fiber_transfer:
        case lauf::asm_op::fiber_suspend:
        case lauf::asm_op::store_local_value:
        // Instructions that we can't remove easily.
        case lauf::asm_op::pop:
        case lauf::asm_op::roll:
        case lauf::asm_op::swap:
        // We never remove pop_top; it was added because we couldn't pop the last time, so why
        // should it be possible now.
        case lauf::asm_op::pop_top:
            // Give up at this point and add actual instructions for popping.
            for (auto i = 0u; i != count; ++i)
                b->cur->insts.push_back(*b, LAUF_BUILD_INST_STACK_IDX(pop_top, 0));
            count = 0;
            break;
        }
    }
}
} // namespace

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
                if (b->locals.size() > 0)
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
        auto begin_offset = ip - insts;
        ip                = block.insts.copy_to(ip);
        auto end_offset   = ip - insts;

        switch (block.terminator)
        {
        case lauf_asm_block::unterminated:
        case lauf_asm_block::fallthrough:
            break;

        case lauf_asm_block::return_:
            if (b->locals.size() > 0)
                *ip++ = LAUF_BUILD_INST_VALUE(local_free, unsigned(b->locals.size()));
            *ip++ = LAUF_BUILD_INST_NONE(return_);
            break;

        case lauf_asm_block::jump:
            if (block.next[0]->offset == end_offset + 1)
                *ip++ = LAUF_BUILD_INST_NONE(nop);
            else
                *ip++ = LAUF_BUILD_INST_OFFSET(jump, block.next[0]->offset - end_offset);
            break;

        case lauf_asm_block::branch2:
            *ip++ = LAUF_BUILD_INST_OFFSET(branch_false, block.next[1]->offset - end_offset);
            ++end_offset;

            if (block.next[0]->offset == end_offset + 1)
                *ip++ = LAUF_BUILD_INST_NONE(nop);
            else
                *ip++ = LAUF_BUILD_INST_OFFSET(jump, block.next[0]->offset - end_offset);
            break;

        case lauf_asm_block::branch3:
            *ip++ = LAUF_BUILD_INST_OFFSET(branch_eq, block.next[1]->offset - end_offset);
            ++end_offset;

            *ip++ = LAUF_BUILD_INST_OFFSET(branch_gt, block.next[2]->offset - end_offset);
            ++end_offset;

            if (block.next[0]->offset == end_offset + 1)
                *ip++ = LAUF_BUILD_INST_NONE(nop);
            else
                *ip++ = LAUF_BUILD_INST_OFFSET(jump, block.next[0]->offset - end_offset);
            break;

        case lauf_asm_block::panic:
            *ip++ = LAUF_BUILD_INST_NONE(panic);
            break;
        }

        for (auto loc : block.debug_locations)
        {
            loc.inst_idx += begin_offset;
            b->mod->inst_debug_locations.push_back(*b->mod, loc);
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

    b->fn->max_cstack_size = sizeof(lauf_runtime_stack_frame) + b->local_allocation_size;

    return !b->errored;
}

lauf_asm_local* lauf_asm_build_local(lauf_asm_builder* b, lauf_asm_layout layout)
{
    if (b->prologue->insts.empty())
        b->prologue->insts.push_back(*b, LAUF_BUILD_INST_VALUE(setup_local_alloc, 1));
    else
        ++b->prologue->insts.front().setup_local_alloc.value;

    layout.size = lauf::round_to_multiple_of_alignment(layout.size, alignof(void*));

    std::uint16_t offset;
    if (layout.alignment <= alignof(void*))
    {
        // Ensure that the stack frame is always aligned to a pointer.
        // This means we can allocate without worrying about alignment.
        layout.alignment = alignof(void*);
        b->prologue->insts.push_back(*b, LAUF_BUILD_INST_LAYOUT(local_alloc, layout));

        // The offset is the current size, we don't need to worry about alignment.
        offset = std::uint16_t(b->local_allocation_size + sizeof(lauf_runtime_stack_frame));
        b->local_allocation_size += layout.size;
    }
    else
    {
        b->prologue->insts.push_back(*b, LAUF_BUILD_INST_LAYOUT(local_alloc_aligned, layout));

        // We need to align it, but don't know the base address of the stack frame yet.
        // We only know that we need at most `layout.alignment` padding bytes*, so reserve that much
        // space.
        //
        // * Actually only `layout.alignment - 1`, but we need to ensure that we're always aligned
        // for a pointer.
        //  Since `layout.alignment` is a multiple of it (as a power of two bigger than it), and
        //  size a multiple of alignment, `layout.alignment + layout.size` is as well.
        b->local_allocation_size += layout.alignment + layout.size;
        // Since we don't know the exact alignment offset, we can't compute it statically.
        offset = UINT16_MAX;
    }

    auto index = b->locals.size();
    return &b->locals.push_back(*b, {layout, std::uint16_t(index), offset});
}

lauf_asm_block* lauf_asm_declare_block(lauf_asm_builder* b, size_t input_count)
{
    LAUF_BUILD_ASSERT(input_count <= UINT8_MAX, "too many input values for block");
    if (b->blocks.size() == 1u)
        LAUF_BUILD_ASSERT(input_count == b->fn->sig.input_count,
                          "requested entry block has different input count from function");

    return &b->blocks.emplace_back(*b, *b, std::uint8_t(input_count));
}

void lauf_asm_build_block(lauf_asm_builder* b, lauf_asm_block* block)
{
    LAUF_BUILD_ASSERT(block->terminator == lauf_asm_block::unterminated,
                      "cannot continue building a block that has been terminated already");

    b->cur = block;
}

size_t lauf_asm_build_get_vstack_size(lauf_asm_builder* b)
{
    LAUF_BUILD_ASSERT_CUR;
    return b->cur->vstack.size();
}

void lauf_asm_build_debug_location(lauf_asm_builder* b, lauf_asm_debug_location loc)
{
    LAUF_BUILD_ASSERT_CUR;

    if (b->cur->debug_locations.empty()
        || b->cur->debug_locations.back().location.line_nr != loc.line_nr
        || b->cur->debug_locations.back().location.column_nr != loc.column_nr)
        b->cur->debug_locations.push_back(*b, {b->fn->function_idx, uint16_t(b->cur->insts.size()),
                                               loc});
}

void lauf_asm_inst_return(lauf_asm_builder* b)
{
    LAUF_BUILD_ASSERT_CUR;

    LAUF_BUILD_ASSERT(b->cur->vstack.finish(b->cur->sig.output_count),
                      "block output count overflow");
    LAUF_BUILD_ASSERT(b->cur->sig.output_count == b->fn->sig.output_count,
                      "requested exit block has different output count from function");

    b->cur->terminator = lauf_asm_block::return_;
    b->cur             = nullptr;
}

void lauf_asm_inst_jump(lauf_asm_builder* b, const lauf_asm_block* dest)
{
    LAUF_BUILD_ASSERT_CUR;

    LAUF_BUILD_ASSERT(b->cur->vstack.finish(b->cur->sig.output_count),
                      "block output count overflow");

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
                      "block output count overflow");

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
                      "block output count overflow");

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

    b->cur->vstack.push(*b, callee->sig.output_count);
}

void lauf_asm_inst_call_indirect(lauf_asm_builder* b, lauf_asm_signature sig)
{
    LAUF_BUILD_ASSERT_CUR;

    LAUF_BUILD_ASSERT(b->cur->vstack.pop(sig.input_count), "missing input values for call");
    LAUF_BUILD_ASSERT(b->cur->vstack.pop(), "missing function address");

    b->cur->insts.push_back(*b, LAUF_BUILD_INST_SIGNATURE(call_indirect, sig.input_count,
                                                          sig.output_count));

    b->cur->vstack.push(*b, sig.output_count);
}

namespace
{
void add_call_builtin(lauf_asm_builder* b, lauf_runtime_builtin_function callee)
{
    auto offset = lauf::compress_pointer_offset(&lauf_runtime_builtin_dispatch, callee.impl);
    if ((callee.flags & LAUF_RUNTIME_BUILTIN_NO_PROCESS) != 0
        && (callee.flags & LAUF_RUNTIME_BUILTIN_NO_PANIC) != 0)
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_OFFSET(call_builtin_no_frame, offset));
    else
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_OFFSET(call_builtin, offset));

    b->cur->vstack.push(*b, callee.output_count);
}
} // namespace

void lauf_asm_inst_call_builtin(lauf_asm_builder* b, lauf_runtime_builtin_function callee)
{
    LAUF_BUILD_ASSERT_CUR;

    bool               all_constant = true;
    lauf_runtime_value vstack[UINT8_MAX];

    // vstack grows down.
    auto vstack_ptr = vstack + UINT8_MAX;
    // We pop arguments in reverse order.
    vstack_ptr -= callee.input_count;

    for (auto i = 0u; i != callee.input_count; ++i)
    {
        auto value = b->cur->vstack.pop();
        LAUF_BUILD_ASSERT(value, "missing input values for call");
        if (value->type == lauf::builder_vstack::value::constant)
        {
            *vstack_ptr = value->as_constant;
            ++vstack_ptr;
        }
        else
            all_constant = false;
    }

    if (all_constant && (callee.flags & LAUF_RUNTIME_BUILTIN_NO_PROCESS) != 0
        && (callee.flags & LAUF_RUNTIME_BUILTIN_CONSTANT_FOLD) != 0)
    {
        assert(vstack_ptr == vstack + UINT8_MAX);
        lauf_asm_inst         code[2] = {LAUF_BUILD_INST_NONE(nop), LAUF_BUILD_INST_NONE(exit)};
        [[maybe_unused]] auto success
            = callee.impl(code, vstack_ptr - callee.input_count, nullptr, nullptr);
        if (success)
        {
            // Pop the input values as the call would.
            add_pop_top_n(b, callee.input_count);

            // Push the results. We start at the top and walk our way down,
            // to get them in the correct oder.
            for (auto i = 0u; i != callee.output_count; ++i)
            {
                --vstack_ptr;
                lauf_asm_inst_uint(b, vstack_ptr->as_uint);
            }
        }
        else
        {
            // It paniced, so we keep the call as-is.
            add_call_builtin(b, callee);
        }
    }
    else
    {
        add_call_builtin(b, callee);
    }
}

void lauf_asm_inst_fiber_create(lauf_asm_builder* b, const lauf_asm_function* callee)
{
    LAUF_BUILD_ASSERT_CUR;

    auto offset = lauf::compress_pointer_offset(b->fn, callee);
    b->cur->insts.push_back(*b, LAUF_BUILD_INST_OFFSET(fiber_create, offset));
    b->cur->vstack.push(*b, 1);
}

void lauf_asm_inst_fiber_resume(lauf_asm_builder* b, lauf_asm_signature sig)
{
    LAUF_BUILD_ASSERT_CUR;

    LAUF_BUILD_ASSERT(b->cur->vstack.pop(sig.input_count), "missing inputs");
    LAUF_BUILD_ASSERT(b->cur->vstack.pop(), "missing handle");
    b->cur->insts.push_back(*b, LAUF_BUILD_INST_SIGNATURE(fiber_resume, sig.input_count,
                                                          sig.output_count));
    b->cur->vstack.push(*b, 1);
    b->cur->vstack.push(*b, sig.output_count);
}

void lauf_asm_inst_fiber_transfer(lauf_asm_builder* b, lauf_asm_signature sig)
{
    LAUF_BUILD_ASSERT_CUR;

    LAUF_BUILD_ASSERT(b->cur->vstack.pop(sig.input_count), "missing inputs");
    LAUF_BUILD_ASSERT(b->cur->vstack.pop(), "missing handle");
    b->cur->insts.push_back(*b, LAUF_BUILD_INST_SIGNATURE(fiber_transfer, sig.input_count,
                                                          sig.output_count));
    b->cur->vstack.push(*b, 1);
    b->cur->vstack.push(*b, sig.output_count);
}

void lauf_asm_inst_fiber_suspend(lauf_asm_builder* b, lauf_asm_signature sig)
{
    LAUF_BUILD_ASSERT_CUR;

    LAUF_BUILD_ASSERT(b->cur->vstack.pop(sig.input_count), "missing inputs");
    b->cur->insts.push_back(*b, LAUF_BUILD_INST_SIGNATURE(fiber_suspend, sig.input_count,
                                                          sig.output_count));
    b->cur->vstack.push(*b, sig.output_count);
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
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(push, value));
    }
    else if ((value & lauf_uint(0xFFFF'0000'0000'0000)) == 0)
    {
        // 0x0000'yyyy'yyxx'xxxx: push + push2
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(push, value & 0xFF'FFFF));
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(push2, value >> 24));
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
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(push, value & 0xFF'FFFF));
        if ((std::uint32_t(value >> 24) & 0xFF'FFFF) != 0)
            b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(push2, (value >> 24) & 0xFF'FFFF));
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(push3, value >> 48));
    }

    b->cur->vstack.push(*b, [&] {
        lauf_runtime_value result;
        result.as_uint = value;
        return result;
    }());
}

void lauf_asm_inst_null(lauf_asm_builder* b)
{
    LAUF_BUILD_ASSERT_CUR;

    // NULL has all bits set.
    b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(pushn, 0));
    b->cur->vstack.push(*b, lauf_runtime_value{});
}

void lauf_asm_inst_global_addr(lauf_asm_builder* b, const lauf_asm_global* global)
{
    LAUF_BUILD_ASSERT_CUR;

    b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(global_addr, global->allocation_idx));
    b->cur->vstack.push(*b, [&] {
        lauf_runtime_value result;
        result.as_address.allocation = global->allocation_idx;
        result.as_address.offset     = 0;
        result.as_address.generation = 0; // Always true for globals.
        return result;
    }());
}

void lauf_asm_inst_local_addr(lauf_asm_builder* b, const lauf_asm_local* local)
{
    LAUF_BUILD_ASSERT_CUR;

    b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(local_addr, local->index));
    b->cur->vstack.push(*b, [&] {
        lauf::builder_vstack::value result;
        result.type     = result.local_addr;
        result.as_local = local;
        return result;
    }());
}

void lauf_asm_inst_function_addr(lauf_asm_builder* b, const lauf_asm_function* function)
{
    LAUF_BUILD_ASSERT_CUR;

    auto offset = lauf::compress_pointer_offset(b->fn, function);
    b->cur->insts.push_back(*b, LAUF_BUILD_INST_OFFSET(function_addr, offset));
    b->cur->vstack.push(*b, [&] {
        lauf_runtime_value result;
        result.as_function_address.index        = function->function_idx;
        result.as_function_address.input_count  = function->sig.input_count;
        result.as_function_address.output_count = function->sig.output_count;
        return result;
    }());
}

void lauf_asm_inst_layout(lauf_asm_builder* b, lauf_asm_layout layout)
{
    lauf_asm_inst_uint(b, layout.alignment);
    lauf_asm_inst_uint(b, layout.size);
}

void lauf_asm_inst_cc(lauf_asm_builder* b, lauf_asm_inst_condition_code cc)
{
    LAUF_BUILD_ASSERT_CUR;

    auto cmp = b->cur->vstack.pop();
    LAUF_BUILD_ASSERT(cmp, "missing cmp");

    if (cmp->type == cmp->constant)
    {
        auto value = cmp->as_constant;
        switch (cc)
        {
        case LAUF_ASM_INST_CC_EQ:
            if (value.as_sint == 0)
                value.as_uint = 1;
            else
                value.as_uint = 0;
            break;
        case LAUF_ASM_INST_CC_NE:
            if (value.as_sint != 0)
                value.as_uint = 1;
            else
                value.as_uint = 0;
            break;
        case LAUF_ASM_INST_CC_LT:
            if (value.as_sint < 0)
                value.as_uint = 1;
            else
                value.as_uint = 0;
            break;
        case LAUF_ASM_INST_CC_LE:
            if (value.as_sint <= 0)
                value.as_uint = 1;
            else
                value.as_uint = 0;
            break;
        case LAUF_ASM_INST_CC_GT:
            if (value.as_sint > 0)
                value.as_uint = 1;
            else
                value.as_uint = 0;
            break;
        case LAUF_ASM_INST_CC_GE:
            if (value.as_sint >= 0)
                value.as_uint = 1;
            else
                value.as_uint = 0;
            break;
        }

        add_pop_top_n(b, 1);
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(push, value.as_uint));
        b->cur->vstack.push(*b, value);
    }
    else
    {
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(cc, unsigned(cc)));
        b->cur->vstack.push(*b, 1);
    }
}

void lauf_asm_inst_pop(lauf_asm_builder* b, uint16_t stack_index)
{
    LAUF_BUILD_ASSERT_CUR;

    LAUF_BUILD_ASSERT(stack_index < b->cur->vstack.size(), "invalid stack index");

    if (stack_index == 0)
        add_pop_top_n(b, 1);
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

    b->cur->vstack.push(*b, b->cur->vstack.pick(stack_index));
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

    b->cur->vstack.roll(stack_index);
}

void lauf_asm_inst_array_element(lauf_asm_builder* b, lauf_asm_layout element_layout)
{
    LAUF_BUILD_ASSERT_CUR;

    auto multiple
        = lauf::round_to_multiple_of_alignment(element_layout.size, element_layout.alignment);

    auto index = b->cur->vstack.pop();
    LAUF_BUILD_ASSERT(index, "missing index");
    LAUF_BUILD_ASSERT(b->cur->vstack.pop(1), "missing address");

    if (index->type == index->constant)
    {
        add_pop_top_n(b, 1);
        auto offset = index->as_constant.as_sint * lauf_sint(multiple);
        if (offset > 0)
            b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(aggregate_member, lauf_uint(offset)));
        b->cur->vstack.push(*b, 1);
    }
    else
    {
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(array_element, multiple));
        b->cur->vstack.push(*b, 1);
    }
}

void lauf_asm_inst_aggregate_member(lauf_asm_builder* b, size_t member_index,
                                    const lauf_asm_layout* member_layouts, size_t member_count)
{
    LAUF_BUILD_ASSERT_CUR;
    LAUF_BUILD_ASSERT(member_index < member_count, "invalid member");

    // The offset is the size of the aggregate that stops at the specified member,
    // but without its size.
    // That way, we get the alignment buffer for the desired member.
    auto layout = lauf_asm_aggregate_layout(member_layouts, member_index + 1);
    auto offset = layout.size - member_layouts[member_index].size;

    if (offset > 0)
    {
        LAUF_BUILD_ASSERT(b->cur->vstack.pop(1), "missing address");
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(aggregate_member, offset));
        b->cur->vstack.push(*b, 1);
    }
}

namespace
{
bool is_valid_local_load_store_value(lauf::builder_vstack::value addr, lauf_asm_type type)
{
    if (type.load_fn != lauf_asm_type_value.load_fn
        || type.store_fn != lauf_asm_type_value.store_fn)
        return false;

    if (addr.type != addr.local_addr)
        return false;

    auto local_layout = addr.as_local->layout;
    if (local_layout.alignment > alignof(void*))
        // Don't know the offset for over aligned data yet.
        return false;

    return local_layout.size >= type.layout.size && local_layout.alignment >= type.layout.alignment;
}
} // namespace

void lauf_asm_inst_load_field(lauf_asm_builder* b, lauf_asm_type type, size_t field_index)
{
    LAUF_BUILD_ASSERT_CUR;
    LAUF_BUILD_ASSERT(field_index < type.field_count, "invalid field index");

    auto addr = b->cur->vstack.pop();
    LAUF_BUILD_ASSERT(addr, "missing address");
    if (is_valid_local_load_store_value(*addr, type))
    {
        add_pop_top_n(b, 1);
        b->cur->insts.push_back(*b,
                                LAUF_BUILD_INST_VALUE(load_local_value, addr->as_local->offset));
        b->cur->vstack.push(*b, 1);
    }
    else
    {
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_LAYOUT(deref_const, type.layout));
        b->cur->vstack.push(*b, 1);

        lauf_asm_inst_uint(b, field_index);

        lauf_runtime_builtin_function builtin{};
        builtin.impl         = type.load_fn;
        builtin.input_count  = 2;
        builtin.output_count = 1;
        lauf_asm_inst_call_builtin(b, builtin);
    }
}

void lauf_asm_inst_store_field(lauf_asm_builder* b, lauf_asm_type type, size_t field_index)
{
    LAUF_BUILD_ASSERT_CUR;
    LAUF_BUILD_ASSERT(field_index < type.field_count, "invalid field index");

    auto addr = b->cur->vstack.pop();
    LAUF_BUILD_ASSERT(addr, "missing address");
    if (is_valid_local_load_store_value(*addr, type))
    {
        add_pop_top_n(b, 1);
        b->cur->insts.push_back(*b,
                                LAUF_BUILD_INST_VALUE(store_local_value, addr->as_local->offset));
        b->cur->vstack.pop(1);
    }
    else
    {
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_LAYOUT(deref_mut, type.layout));
        b->cur->vstack.push(*b, 1);

        lauf_asm_inst_uint(b, field_index);

        lauf_runtime_builtin_function builtin{};
        builtin.impl         = type.store_fn;
        builtin.input_count  = 3;
        builtin.output_count = 0;
        lauf_asm_inst_call_builtin(b, builtin);
    }
}

