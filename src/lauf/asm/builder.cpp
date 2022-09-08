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
        case lauf::asm_op::return_free:
        case lauf::asm_op::jump:
        case lauf::asm_op::branch_eq:
        case lauf::asm_op::branch_ne:
        case lauf::asm_op::branch_lt:
        case lauf::asm_op::branch_le:
        case lauf::asm_op::branch_ge:
        case lauf::asm_op::branch_gt:
        case lauf::asm_op::panic:
        case lauf::asm_op::exit:
        case lauf::asm_op::setup_local_alloc:
        case lauf::asm_op::local_alloc:
        case lauf::asm_op::local_alloc_aligned:
        case lauf::asm_op::local_storage:
            assert(false && "not added at this point");
            break;

        case lauf::asm_op::local_addr: {
            auto inst = b->cur->insts.back();
            for (auto& local : b->locals)
                if (local.index == inst.local_addr.value)
                {
                    --local.address_count;
                    break;
                }
            // fallthrough
        }
        case lauf::asm_op::push:
        case lauf::asm_op::pushn:
        case lauf::asm_op::global_addr:
        case lauf::asm_op::function_addr:
        case lauf::asm_op::pick:
        case lauf::asm_op::dup:
        case lauf::asm_op::load_local_value:
        case lauf::asm_op::load_global_value:
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
        case lauf::asm_op::call_builtin_no_regs:
        case lauf::asm_op::fiber_resume:
        case lauf::asm_op::fiber_transfer:
        case lauf::asm_op::fiber_suspend:
        case lauf::asm_op::store_local_value:
        case lauf::asm_op::store_global_value:
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

namespace
{
std::size_t create_prologue(lauf_asm_builder* b)
{
    if (b->locals.empty())
        return 0;

    auto prologue               = b->prologue;
    auto local_allocation_count = [&] {
        auto result = 0u;
        for (auto& local : b->locals)
            if (local.address_count > 0)
                ++result;
        return result;
    }();

    if (local_allocation_count > 0)
        prologue->insts.push_back(*b,
                                  LAUF_BUILD_INST_VALUE(setup_local_alloc, local_allocation_count));

    for (auto& local : b->locals)
    {
        assert(local.layout.alignment >= alignof(void*));
        if (local.address_count == 0)
        {
            auto space = local.layout.size;
            if (local.layout.alignment > alignof(void*))
                space += local.layout.alignment;

            // Note that this will simply bump the stack space and not compute the correct
            // offset for over-aligned data. However, we do not promote over-aligned locals to
            // load/store_local_value, so if they're accessed, they have their address taken.
            //
            // The only way an over-aligned local ends up here is if it's unused,
            // in which case we need to reserve the space to keep offsets correct,
            // but don't care where exactly it lives in the memory.
            prologue->insts.push_back(*b, LAUF_BUILD_INST_VALUE(local_storage, space));
        }
        else
        {
            if (local.layout.alignment == alignof(void*))
                b->prologue->insts.push_back(*b, LAUF_BUILD_INST_LAYOUT(local_alloc, local.layout));
            else
                b->prologue->insts.push_back(*b, LAUF_BUILD_INST_LAYOUT(local_alloc_aligned,
                                                                        local.layout));
        }
    }

    return local_allocation_count;
}

void mark_reachable_blocks(lauf_asm_builder* b)
{
    auto mark = [&](auto recurse, const lauf_asm_block* cur) {
        if (cur->reachable)
            return;

        const_cast<lauf_asm_block*>(cur)->reachable = true;

        switch (cur->terminator)
        {
        case lauf_asm_block::unterminated:
        case lauf_asm_block::return_:
        case lauf_asm_block::panic:
            break;
        case lauf_asm_block::fallthrough:
            // Only the prologue uses it, which is already dealt with.
            assert(false);
            break;

        case lauf_asm_block::jump:
            recurse(recurse, cur->next[0]);
            break;
        case lauf_asm_block::branch_ne_eq:
        case lauf_asm_block::branch_lt_ge:
        case lauf_asm_block::branch_le_gt:
            recurse(recurse, cur->next[0]);
            recurse(recurse, cur->next[1]);
            break;
        }
    };

    b->prologue->reachable = true;
    if (b->blocks.size() == 1)
        return;

    // The real entry block is reachable.
    auto entry = b->blocks.begin();
    ++entry;
    mark(mark, &*entry);
}

template <typename Iterator, typename Sink>
void generate_terminator(const char* context, lauf_asm_builder* b, Iterator block, Iterator end,
                         std::size_t local_allocation_count, Sink sink)
{
    auto next_block = [&] {
        auto next_iter = block;
        do
        {
            ++next_iter;
        } while (next_iter != end && !next_iter->reachable);
        return next_iter == end ? nullptr : &*next_iter;
    }();

    switch (block->terminator)
    {
    case lauf_asm_block::unterminated:
        // We allow unterminated blocks that we haven't actually built yet.
        if (!block->insts.empty())
            b->error(context, "unterminated block");
        break;

    case lauf_asm_block::fallthrough:
        break;

    case lauf_asm_block::return_:
        if (local_allocation_count > 0)
            sink(lauf::asm_op::return_free, nullptr);
        else
            sink(lauf::asm_op::return_, nullptr);
        break;
    case lauf_asm_block::panic:
        sink(lauf::asm_op::panic, nullptr);
        break;

    case lauf_asm_block::jump:
        if (block->next[0] != next_block)
            sink(lauf::asm_op::jump, block->next[0]);
        break;
    case lauf_asm_block::branch_ne_eq:
        if (block->next[0] == next_block)
        {
            sink(lauf::asm_op::branch_eq, block->next[1]);
        }
        else if (block->next[1] == next_block)
        {
            sink(lauf::asm_op::branch_ne, block->next[0]);
        }
        else
        {
            sink(lauf::asm_op::branch_eq, block->next[1]);
            sink(lauf::asm_op::jump, block->next[0]);
        }
        break;
    case lauf_asm_block::branch_lt_ge:
        if (block->next[0] == next_block)
        {
            sink(lauf::asm_op::branch_ge, block->next[1]);
        }
        else if (block->next[1] == next_block)
        {
            sink(lauf::asm_op::branch_lt, block->next[0]);
        }
        else
        {
            sink(lauf::asm_op::branch_ge, block->next[1]);
            sink(lauf::asm_op::jump, block->next[0]);
        }
        break;
    case lauf_asm_block::branch_le_gt:
        if (block->next[0] == next_block)
        {
            sink(lauf::asm_op::branch_gt, block->next[1]);
        }
        else if (block->next[1] == next_block)
        {
            sink(lauf::asm_op::branch_le, block->next[0]);
        }
        else
        {
            sink(lauf::asm_op::branch_gt, block->next[1]);
            sink(lauf::asm_op::jump, block->next[0]);
        }
        break;
    }
}

void generate_bytecode(const char* context, lauf_asm_builder* b, std::size_t local_allocation_count)
{
    auto insts_count = [&] {
        auto result = std::size_t(0);
        for (auto block = b->blocks.begin(); block != b->blocks.end(); ++block)
        {
            if (!block->reachable)
                continue;

            block->offset = std::uint16_t(result);
            result += block->insts.size();
            generate_terminator(context, b, block, b->blocks.end(), local_allocation_count,
                                [&](lauf::asm_op, const lauf_asm_block*) { ++result; });
        }
        return result;
    }();

    auto insts = b->mod->allocate<lauf_asm_inst>(insts_count);
    auto ip    = insts;
    for (auto block = b->blocks.begin(); block != b->blocks.end(); ++block)
    {
        if (!block->reachable)
            continue;

        ip = block->insts.copy_to(ip);

        generate_terminator(context, b, block, b->blocks.end(), local_allocation_count,
                            [&](lauf::asm_op op, const lauf_asm_block* dest) {
                                ip[0].nop.op = op;
                                switch (op)
                                {
                                case lauf::asm_op::return_:
                                case lauf::asm_op::panic:
                                    break;

                                case lauf::asm_op::return_free:
                                    ip[0].return_free.value = std::uint32_t(local_allocation_count);
                                    break;

                                case lauf::asm_op::jump:
                                case lauf::asm_op::branch_eq:
                                case lauf::asm_op::branch_ne:
                                case lauf::asm_op::branch_lt:
                                case lauf::asm_op::branch_le:
                                case lauf::asm_op::branch_ge:
                                case lauf::asm_op::branch_gt:
                                    assert(dest != nullptr);
                                    ip[0].jump.offset = std::int32_t(dest->offset - (ip - insts));
                                    break;

                                default:
                                    assert(false);
                                    break;
                                }
                                ++ip;
                            });

        for (auto loc : block->debug_locations)
        {
            loc.inst_idx += block->offset;
            b->mod->inst_debug_locations.push_back(*b->mod, loc);
        }
    } // namespace

    b->fn->insts       = insts;
    b->fn->insts_count = std::uint16_t(insts_count);
    if (b->fn->insts_count != insts_count)
        b->error(context, "too many instructions");
}

void finalize_function(const char* context, lauf_asm_builder* b)
{
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
}
} // namespace

bool lauf_asm_build_finish(lauf_asm_builder* b)
{
    constexpr auto context = LAUF_BUILD_ASSERT_CONTEXT;

    mark_reachable_blocks(b);

    auto local_allocation_count = create_prologue(b);
    generate_bytecode(context, b, local_allocation_count);
    finalize_function(context, b);
    return !b->errored;
}

lauf_asm_local* lauf_asm_build_local(lauf_asm_builder* b, lauf_asm_layout layout)
{
    layout.size = lauf::round_to_multiple_of_alignment(layout.size, alignof(void*));

    std::uint16_t offset;
    if (layout.alignment <= alignof(void*))
    {
        // Ensure that the stack frame is always aligned to a pointer.
        // This means we can allocate without worrying about alignment.
        layout.alignment = alignof(void*);

        // The offset is the current size, we don't need to worry about alignment.
        offset = std::uint16_t(b->local_allocation_size + sizeof(lauf_runtime_stack_frame));
        b->local_allocation_size += layout.size;
    }
    else
    {
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
    return &b->locals.push_back(*b, {layout, std::uint16_t(index), offset, 0});
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
    if (b->cur == nullptr)
        return 0;

    return b->cur->vstack.size();
}

void lauf_asm_build_debug_location(lauf_asm_builder* b, lauf_asm_debug_location loc)
{
    LAUF_BUILD_CHECK_CUR;

    if (b->cur->debug_locations.empty()
        || b->cur->debug_locations.back().location.line_nr != loc.line_nr
        || b->cur->debug_locations.back().location.column_nr != loc.column_nr)
        b->cur->debug_locations.push_back(*b, {b->fn->function_idx, uint16_t(b->cur->insts.size()),
                                               loc});
}

void lauf_asm_inst_return(lauf_asm_builder* b)
{
    LAUF_BUILD_CHECK_CUR;

    LAUF_BUILD_ASSERT(b->cur->vstack.finish(b->cur->sig.output_count),
                      "block output count overflow");
    LAUF_BUILD_ASSERT(b->cur->sig.output_count == b->fn->sig.output_count,
                      "requested exit block has different output count from function");

    b->cur->terminator = lauf_asm_block::return_;
    b->cur             = nullptr;
}

void lauf_asm_inst_jump(lauf_asm_builder* b, const lauf_asm_block* dest)
{
    LAUF_BUILD_CHECK_CUR;

    LAUF_BUILD_ASSERT(b->cur->vstack.finish(b->cur->sig.output_count),
                      "block output count overflow");

    LAUF_BUILD_ASSERT(b->cur->sig.output_count == dest->sig.input_count,
                      "jump target's input count not compatible with current block's output count");
    b->cur->terminator = lauf_asm_block::jump;
    b->cur->next[0]    = dest;
    b->cur             = nullptr;
}

const lauf_asm_block* lauf_asm_inst_branch(lauf_asm_builder* b, const lauf_asm_block* if_true,
                                           const lauf_asm_block* if_false)
{
    if (b->cur == nullptr)
        return nullptr;

    auto condition = b->cur->vstack.pop();
    LAUF_BUILD_ASSERT(condition, "missing condition");
    LAUF_BUILD_ASSERT(b->cur->vstack.finish(b->cur->sig.output_count),
                      "block output count overflow");

    LAUF_BUILD_ASSERT(
        b->cur->sig.output_count == if_true->sig.input_count,
        "branch target's input count not compatible with current block's output count");
    LAUF_BUILD_ASSERT(
        b->cur->sig.output_count == if_false->sig.input_count,
        "branch target's input count not compatible with current block's output count");

    const lauf_asm_block* next_block = nullptr;
    if (if_true == if_false)
    {
        add_pop_top_n(b, 1);
        b->cur->terminator = lauf_asm_block::jump;
        b->cur->next[0]    = if_true;

        next_block = b->cur->next[0];
    }
    else if (condition->type == condition->constant)
    {
        add_pop_top_n(b, 1);
        b->cur->terminator = lauf_asm_block::jump;
        b->cur->next[0]    = condition->as_constant.as_uint != 0 ? if_true : if_false;

        next_block = b->cur->next[0];
    }
    else if (!b->cur->insts.empty() && b->cur->insts.back().op() == lauf::asm_op::cc)
    {
        // Remove the cc instruction.
        auto cc = b->cur->insts.back().cc.value;
        b->cur->insts.pop_back();

        switch (cc)
        {
        case LAUF_ASM_INST_CC_EQ:
            b->cur->terminator = lauf_asm_block::branch_ne_eq;
            b->cur->next[0]    = if_false;
            b->cur->next[1]    = if_true;
            break;
        case LAUF_ASM_INST_CC_NE:
            b->cur->terminator = lauf_asm_block::branch_ne_eq;
            b->cur->next[0]    = if_true;
            b->cur->next[1]    = if_false;
            break;
        case LAUF_ASM_INST_CC_LT:
            b->cur->terminator = lauf_asm_block::branch_lt_ge;
            b->cur->next[0]    = if_true;
            b->cur->next[1]    = if_false;
            break;
        case LAUF_ASM_INST_CC_LE:
            b->cur->terminator = lauf_asm_block::branch_le_gt;
            b->cur->next[0]    = if_true;
            b->cur->next[1]    = if_false;
            break;
        case LAUF_ASM_INST_CC_GT:
            b->cur->terminator = lauf_asm_block::branch_le_gt;
            b->cur->next[0]    = if_false;
            b->cur->next[1]    = if_true;
            break;
        case LAUF_ASM_INST_CC_GE:
            b->cur->terminator = lauf_asm_block::branch_lt_ge;
            b->cur->next[0]    = if_false;
            b->cur->next[1]    = if_true;
            break;
        }
    }
    else
    {
        b->cur->terminator = lauf_asm_block::branch_ne_eq;
        b->cur->next[0]    = if_true;  // true != 0 means ne
        b->cur->next[1]    = if_false; // false = 0 means eq
    }

    b->cur = nullptr;
    return next_block;
}

void lauf_asm_inst_panic(lauf_asm_builder* b)
{
    LAUF_BUILD_CHECK_CUR;

    LAUF_BUILD_ASSERT(b->cur->vstack.pop(), "missing message");

    b->cur->terminator = lauf_asm_block::panic;
    b->cur             = nullptr;
}

void lauf_asm_inst_call(lauf_asm_builder* b, const lauf_asm_function* callee)
{
    LAUF_BUILD_CHECK_CUR;

    LAUF_BUILD_ASSERT(b->cur->vstack.pop(callee->sig.input_count), "missing input values for call");

    auto offset = lauf::compress_pointer_offset(b->fn, callee);
    b->cur->insts.push_back(*b, LAUF_BUILD_INST_OFFSET(call, offset));

    b->cur->vstack.push(*b, callee->sig.output_count);
}

namespace
{
lauf_asm_function* get_constant_function(lauf_asm_module* mod, lauf::builder_vstack::value value,
                                         lauf_asm_signature sig)
{
    if (value.type != value.constant)
        return nullptr;

    auto addr = value.as_constant.as_function_address;
    if (addr.input_count != sig.input_count || addr.output_count != sig.output_count)
        return nullptr;

    for (auto fn = mod->functions; fn != nullptr; fn = fn->next)
        if (fn->function_idx == addr.index)
            return fn;

    return nullptr;
}
} // namespace

void lauf_asm_inst_call_indirect(lauf_asm_builder* b, lauf_asm_signature sig)
{
    LAUF_BUILD_CHECK_CUR;

    auto fn_addr = b->cur->vstack.pop();
    LAUF_BUILD_ASSERT(fn_addr, "missing function address");
    LAUF_BUILD_ASSERT(b->cur->vstack.pop(sig.input_count), "missing input values for call");

    if (auto callee = get_constant_function(b->mod, *fn_addr, sig))
    {
        add_pop_top_n(b, 1);
        auto offset = lauf::compress_pointer_offset(b->fn, callee);
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_OFFSET(call, offset));
    }
    else
    {
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_SIGNATURE(call_indirect, sig.input_count,
                                                              sig.output_count));
    }

    b->cur->vstack.push(*b, sig.output_count);
}

namespace
{
void add_call_builtin(lauf_asm_builder* b, lauf_runtime_builtin_function callee)
{
    auto offset = lauf::compress_pointer_offset(&lauf_runtime_builtin_dispatch, callee.impl);
    if ((callee.flags & LAUF_RUNTIME_BUILTIN_NO_PROCESS) != 0
        && (callee.flags & LAUF_RUNTIME_BUILTIN_NO_PANIC) != 0)
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_OFFSET(call_builtin_no_regs, offset));
    else
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_OFFSET(call_builtin, offset));

    b->cur->vstack.push(*b, callee.output_count);
}
} // namespace

void lauf_asm_inst_call_builtin(lauf_asm_builder* b, lauf_runtime_builtin_function callee)
{
    LAUF_BUILD_CHECK_CUR;

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

void lauf_asm_inst_fiber_resume(lauf_asm_builder* b, lauf_asm_signature sig)
{
    LAUF_BUILD_CHECK_CUR;

    LAUF_BUILD_ASSERT(b->cur->vstack.pop(sig.input_count), "missing inputs");
    LAUF_BUILD_ASSERT(b->cur->vstack.pop(), "missing handle");
    b->cur->insts.push_back(*b, LAUF_BUILD_INST_SIGNATURE(fiber_resume, sig.input_count,
                                                          sig.output_count));
    b->cur->vstack.push(*b, sig.output_count);
}

void lauf_asm_inst_fiber_transfer(lauf_asm_builder* b, lauf_asm_signature sig)
{
    LAUF_BUILD_CHECK_CUR;

    LAUF_BUILD_ASSERT(b->cur->vstack.pop(sig.input_count), "missing inputs");
    LAUF_BUILD_ASSERT(b->cur->vstack.pop(), "missing handle");
    b->cur->insts.push_back(*b, LAUF_BUILD_INST_SIGNATURE(fiber_transfer, sig.input_count,
                                                          sig.output_count));
    b->cur->vstack.push(*b, sig.output_count);
}

void lauf_asm_inst_fiber_suspend(lauf_asm_builder* b, lauf_asm_signature sig)
{
    LAUF_BUILD_CHECK_CUR;

    LAUF_BUILD_ASSERT(b->cur->vstack.pop(sig.input_count), "missing inputs");
    b->cur->insts.push_back(*b, LAUF_BUILD_INST_SIGNATURE(fiber_suspend, sig.input_count,
                                                          sig.output_count));
    b->cur->vstack.push(*b, sig.output_count);
}

void lauf_asm_inst_sint(lauf_asm_builder* b, lauf_sint value)
{
    LAUF_BUILD_CHECK_CUR;

    // We treat negative values as large positive values.
    lauf_asm_inst_uint(b, lauf_uint(value));
}

void lauf_asm_inst_uint(lauf_asm_builder* b, lauf_uint value)
{
    LAUF_BUILD_CHECK_CUR;

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
    LAUF_BUILD_CHECK_CUR;

    // NULL has all bits set.
    b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(pushn, 0));
    b->cur->vstack.push(*b, lauf_runtime_value{});
}

void lauf_asm_inst_global_addr(lauf_asm_builder* b, const lauf_asm_global* global)
{
    LAUF_BUILD_CHECK_CUR;

    b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(global_addr, global->allocation_idx));
    b->cur->vstack.push(*b, [&] {
        lauf_runtime_value result;
        result.as_address.allocation = global->allocation_idx;
        result.as_address.offset     = 0;
        result.as_address.generation = 0; // Always true for globals.
        return result;
    }());
}

void lauf_asm_inst_local_addr(lauf_asm_builder* b, lauf_asm_local* local)
{
    LAUF_BUILD_CHECK_CUR;

    ++local->address_count;

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
    LAUF_BUILD_CHECK_CUR;

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
    LAUF_BUILD_CHECK_CUR;

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
    LAUF_BUILD_CHECK_CUR;

    LAUF_BUILD_ASSERT(stack_index < b->cur->vstack.size(), "invalid stack index");

    if (stack_index == 0)
        add_pop_top_n(b, 1);
    else
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_STACK_IDX(pop, stack_index));

    b->cur->vstack.pop();
}

void lauf_asm_inst_pick(lauf_asm_builder* b, uint16_t stack_index)
{
    LAUF_BUILD_CHECK_CUR;

    LAUF_BUILD_ASSERT(stack_index < b->cur->vstack.size(), "invalid stack index");

    if (stack_index == 0)
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_STACK_IDX(dup, stack_index));
    else
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_STACK_IDX(pick, stack_index));

    b->cur->vstack.push(*b, b->cur->vstack.pick(stack_index));
}

void lauf_asm_inst_roll(lauf_asm_builder* b, uint16_t stack_index)
{
    LAUF_BUILD_CHECK_CUR;

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
    LAUF_BUILD_CHECK_CUR;

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
    LAUF_BUILD_CHECK_CUR;
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
enum load_store_constant
{
    load_store_dynamic,
    load_store_local,
    load_store_global,
};

load_store_constant load_store_constant_folding(lauf_asm_module*            mod,
                                                lauf::builder_vstack::value addr,
                                                lauf_asm_type type, bool store)
{
    if (type.load_fn != lauf_asm_type_value.load_fn
        || type.store_fn != lauf_asm_type_value.store_fn)
        return load_store_dynamic;

    if (addr.type == addr.local_addr)
    {
        auto local_layout = addr.as_local->layout;
        if (local_layout.alignment > alignof(void*))
            // Don't know the offset for over aligned data yet.
            return load_store_dynamic;

        if (local_layout.size < type.layout.size || local_layout.alignment < type.layout.alignment)
            return load_store_dynamic;

        return load_store_local;
    }
    else if (addr.type == addr.constant)
    {
        auto constant_addr = addr.as_constant.as_address;
        if (constant_addr.allocation >= mod->globals_count && constant_addr.generation != 0
            && constant_addr.offset != 0)
            return load_store_dynamic;

        for (auto global = mod->globals; global != nullptr; global = global->next)
            if (global->allocation_idx == constant_addr.allocation)
            {
                if (store && global->perms == lauf_asm_global::read_only)
                    return load_store_dynamic;

                if (global->size < type.layout.size || global->alignment < type.layout.alignment)
                    return load_store_dynamic;

                return load_store_global;
            }
    }

    return load_store_dynamic;
}
} // namespace

void lauf_asm_inst_load_field(lauf_asm_builder* b, lauf_asm_type type, size_t field_index)
{
    LAUF_BUILD_CHECK_CUR;
    LAUF_BUILD_ASSERT(field_index < type.field_count, "invalid field index");

    auto addr = b->cur->vstack.pop();
    LAUF_BUILD_ASSERT(addr, "missing address");

    auto constant_folding = load_store_constant_folding(b->mod, *addr, type, false);
    if (constant_folding == load_store_local)
    {
        add_pop_top_n(b, 1);
        b->cur->insts.push_back(*b,
                                LAUF_BUILD_INST_VALUE(load_local_value, addr->as_local->offset));
        b->cur->vstack.push(*b, 1);
    }
    else if (constant_folding == load_store_global)
    {
        add_pop_top_n(b, 1);
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(load_global_value,
                                                          addr->as_constant.as_address.allocation));
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
    LAUF_BUILD_CHECK_CUR;
    LAUF_BUILD_ASSERT(field_index < type.field_count, "invalid field index");

    auto addr = b->cur->vstack.pop();
    LAUF_BUILD_ASSERT(addr, "missing address");

    auto constant_folding = load_store_constant_folding(b->mod, *addr, type, true);
    if (constant_folding == load_store_local)
    {
        add_pop_top_n(b, 1);
        b->cur->insts.push_back(*b,
                                LAUF_BUILD_INST_VALUE(store_local_value, addr->as_local->offset));
        b->cur->vstack.pop(1);
    }
    else if (constant_folding == load_store_global)
    {
        add_pop_top_n(b, 1);
        b->cur->insts.push_back(*b, LAUF_BUILD_INST_VALUE(store_global_value,
                                                          addr->as_constant.as_address.allocation));
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

