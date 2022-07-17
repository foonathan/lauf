// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_ASM_BUILDER_HPP_INCLUDED
#define SRC_LAUF_ASM_BUILDER_HPP_INCLUDED

#include <lauf/asm/builder.h>
#include <lauf/asm/instruction.hpp>
#include <lauf/asm/module.h>
#include <lauf/support/arena.hpp>
#include <lauf/support/array_list.hpp>

//=== vstack_size_checker ===//
namespace lauf
{
class vstack_size_checker
{
public:
    explicit vstack_size_checker(std::size_t input_count) : _cur(input_count), _max(input_count) {}

    std::size_t size() const
    {
        return _cur;
    }

    void push(std::size_t n = 1)
    {
        _cur += n;
        if (_cur > _max)
            _max = _cur;
    }

    bool pop(std::size_t n = 1)
    {
        if (_cur < n)
        {
            _cur = 0;
            return false;
        }
        else
        {
            _cur -= n;
            return true;
        }
    }

    bool finish(std::size_t output_count)
    {
        return _cur == output_count;
    }

private:
    std::size_t _cur;
    std::size_t _max;
};
} // namespace lauf

//=== types ===//
struct lauf_asm_block
{
    lauf_asm_signature        sig;
    lauf::vstack_size_checker vstack;

    std::ptrdiff_t                   offset;
    lauf::array_list<lauf::asm_inst> insts;

    enum
    {
        unterminated,
        return_,
        jump,
        branch2,
        branch3,
        panic,
    } terminator;
    lauf_asm_block* next[3];

    explicit lauf_asm_block(lauf_asm_signature sig)
    : sig(sig), vstack(sig.input_count), terminator(unterminated), next{}
    {}
};

struct lauf_asm_builder : lauf::intrinsic_arena<lauf_asm_builder>
{
    lauf_asm_build_options options;
    lauf_asm_module*       mod = nullptr;
    lauf_asm_function*     fn  = nullptr;

    lauf::array_list<lauf_asm_block> blocks;
    lauf_asm_block*                  cur = nullptr;

    bool errored = false;

    explicit lauf_asm_builder(lauf::arena_key key, lauf_asm_build_options options)
    : lauf::intrinsic_arena<lauf_asm_builder>(key), options(options)
    {}

    void error(const char* context, const char* msg);

    void reset(lauf_asm_module* mod, lauf_asm_function* fn)
    {
        this->mod = mod;
        this->fn  = fn;

        blocks.clear();
        cur = nullptr;

        errored = false;

        this->clear();
    }
};

//=== assertions ===//
// Get the function name after lauf_asm_XXX()
#define LAUF_BUILD_ASSERT_CONTEXT (__func__ + 9)

#define LAUF_BUILD_ASSERT(Cond, Msg)                                                               \
    do                                                                                             \
    {                                                                                              \
        if (!(Cond))                                                                               \
            b->error(LAUF_BUILD_ASSERT_CONTEXT, Msg);                                              \
    } while (0)

#define LAUF_BUILD_ASSERT_CUR LAUF_BUILD_ASSERT(b->cur != nullptr, "no current block to build")

//=== instruction building ===//
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
    }(LAUF_BUILD_ASSERT_CONTEXT, static_cast<std::int64_t>(Offset))

#define LAUF_BUILD_INST_STACK_IDX(Name, Idx)                                                       \
    [&] {                                                                                          \
        lauf::asm_inst result;                                                                     \
        result.Name = {lauf::asm_op::Name, Idx};                                                   \
        return result;                                                                             \
    }()

#define LAUF_BUILD_INST_VALUE(Name, Value)                                                         \
    [&] {                                                                                          \
        lauf::asm_inst result;                                                                     \
        result.Name = {lauf::asm_op::Name, Value};                                                 \
        return result;                                                                             \
    }()

#endif // SRC_LAUF_ASM_BUILDER_HPP_INCLUDED

