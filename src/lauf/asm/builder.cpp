// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/asm/builder.hpp>

#include <cstdio>
#include <cstdlib>

#include <lauf/asm/module.hpp>

#define LAUF_BUILD_ASSERT_CONTEXT (__func__ + 9)

#define LAUF_BUILD_ASSERT(Cond, Msg)                                                               \
    do                                                                                             \
    {                                                                                              \
        if (!(Cond))                                                                               \
            b->error(LAUF_BUILD_ASSERT_CONTEXT, Msg);                                              \
    } while (0)

#define LAUF_BUILD_ASSERT_CUR LAUF_BUILD_ASSERT(b->cur != nullptr, "no current block to build")

#define LAUF_BUILD_INST_NONE(Name)                                                                 \
    [&] {                                                                                          \
        lauf::asm_inst result;                                                                     \
        result.Name = {lauf::asm_op::Name};                                                        \
        return result;                                                                             \
    }()

#define LAUF_BUILD_INST_OFFSET(Name, Offset)                                                       \
    [&](const char* context, std::ptrdiff_t offset) {                                              \
        lauf::asm_inst result;                                                                     \
        result.Name = {lauf::asm_op::Name, std::int32_t(offset)};                                  \
        if (result.Name.offset != offset)                                                          \
            b->error(context, "offset too big");                                                   \
        return result;                                                                             \
    }(LAUF_BUILD_ASSERT_CONTEXT, Offset)

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
    return new lauf_asm_builder(options);
}

void lauf_asm_destroy_builder(lauf_asm_builder* b)
{
    delete b;
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

            switch (block.terminator)
            {
            case lauf_asm_block::unterminated:
                b->error(context, "unterminated block");
                break;

            case lauf_asm_block::return_:
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

    auto insts = b->mod->allocate<lauf::asm_inst>(insts_count);

    auto ip = insts;
    for (auto& block : b->blocks)
    {
        auto cur_offset = ip - insts;
        switch (block.terminator)
        {
        case lauf_asm_block::unterminated:
            break;

        case lauf_asm_block::return_:
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

    return !b->errored;
}

lauf_asm_block* lauf_asm_declare_block(lauf_asm_builder* b, lauf_asm_signature sig)
{
    if (b->blocks.empty())
        LAUF_BUILD_ASSERT(sig.input_count == b->fn->sig.input_count,
                          "requested entry block has different input count from function");

    return &b->blocks.emplace_back(sig);
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

void lauf_asm_inst_jump(lauf_asm_builder* b, lauf_asm_block* dest)
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

void lauf_asm_inst_branch2(lauf_asm_builder* b, lauf_asm_block* if_true, lauf_asm_block* if_false)
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
    b->cur->terminator = lauf_asm_block::branch2;
    b->cur->next[0]    = if_true;
    b->cur->next[1]    = if_false;
    b->cur             = nullptr;
}

void lauf_asm_inst_branch3(lauf_asm_builder* b, lauf_asm_block* if_lt, lauf_asm_block* if_eq,
                           lauf_asm_block* if_false)
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
    b->cur->terminator = lauf_asm_block::branch3;
    b->cur->next[0]    = if_lt;
    b->cur->next[1]    = if_eq;
    b->cur->next[2]    = if_false;
    b->cur             = nullptr;
}

void lauf_asm_inst_panic(lauf_asm_builder* b)
{
    LAUF_BUILD_ASSERT_CUR;

    LAUF_BUILD_ASSERT(b->cur->vstack.pop(), "missing message");

    b->cur->terminator = lauf_asm_block::panic;
    b->cur             = nullptr;
}

