// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_ASM_BUILDER_HPP_INCLUDED
#define SRC_LAUF_ASM_BUILDER_HPP_INCLUDED

#include <lauf/asm/builder.h>
#include <lauf/asm/instruction.hpp>
#include <lauf/asm/module.hpp>
#include <lauf/asm/type.h>
#include <lauf/runtime/value.h>
#include <lauf/support/arena.hpp>
#include <lauf/support/array_list.hpp>
#include <optional>

//=== vstack ===//
namespace lauf
{
class builder_vstack
{
public:
    struct value
    {
        enum
        {
            unknown,
            constant,
            local_addr,
        } type;
        union
        {
            char                  as_unknown;
            lauf_runtime_value    as_constant;
            const lauf_asm_local* as_local;
        };
    };

    explicit builder_vstack(arena_base& arena, std::size_t input_count) : _max(input_count)
    {
        for (auto i = 0u; i != input_count; ++i)
            _stack.push_back(arena, {value::unknown, {}});
    }

    std::size_t size() const
    {
        return _stack.size();
    }
    std::size_t max_size() const
    {
        return _max;
    }

    void push(arena_base& arena, value v)
    {
        _stack.push_back(arena, v);
        if (size() > _max)
            _max = size();
    }
    void push(arena_base& arena, std::size_t n)
    {
        for (auto i = 0u; i != n; ++i)
            push(arena, {value::unknown, {}});
    }
    void push(arena_base& arena, lauf_runtime_value constant)
    {
        value v;
        v.type        = value::constant;
        v.as_constant = constant;
        push(arena, v);
    }

    [[nodiscard]] std::optional<value> pop()
    {
        if (_stack.empty())
            return std::nullopt;

        auto result = _stack.back();
        _stack.pop_back();
        return result;
    }
    [[nodiscard]] bool pop(std::size_t n)
    {
        for (auto i = 0u; i != n; ++i)
        {
            if (_stack.empty())
                return false;

            _stack.pop_back();
        }

        return true;
    }

    value pick(std::size_t stack_idx)
    {
        auto cur = _stack.end();
        --cur;
        for (auto i = 0u; i != stack_idx; ++i)
            --cur;
        return *cur;
    }

    void roll(std::size_t stack_idx)
    {
        auto cur = _stack.end();
        --cur;
        for (auto i = 0u; i != stack_idx; ++i)
            --cur;

        auto save = *cur;
        while (true)
        {
            auto next = cur;
            ++next;

            if (next == _stack.end())
                break;
            *cur = *next;
            cur  = next;
        }
        *cur = save;
    }

    bool finish(uint8_t& output_count)
    {
        if (size() > UCHAR_MAX)
            return false;

        output_count = uint8_t(size());
        return true;
    }

private:
    lauf::array_list<value> _stack;
    std::size_t             _max;
};
} // namespace lauf

//=== types ===//
struct lauf_asm_block
{
    lauf_asm_signature   sig;
    lauf::builder_vstack vstack;

    std::uint16_t                               offset    = 0;
    bool                                        reachable = false;
    lauf::array_list<lauf_asm_inst>             insts;
    lauf::array_list<lauf::inst_debug_location> debug_locations;

    enum
    {
        unterminated,
        return_,
        jump,
        branch_ne_eq,
        branch_lt_ge,
        branch_le_gt,
        panic,
    } terminator;
    const lauf_asm_block* next[2];

    explicit lauf_asm_block(lauf::arena_base& arena, uint8_t input_count)
    : sig{input_count, 0}, vstack(arena, input_count), terminator(unterminated), next{}
    {}
};

struct lauf_asm_local
{
    lauf_asm_layout layout;
    std::uint16_t   index;
    // UINT16_MAX if unknown for layout.alignment > alignof(void*)
    std::uint16_t offset;
    // How often its address was taken in the function.
    std::uint16_t address_count;
};

struct lauf_asm_builder : lauf::intrinsic_arena<lauf_asm_builder>
{
    lauf_asm_build_options options;
    lauf_asm_module*       mod   = nullptr;
    lauf_asm_function*     fn    = nullptr;
    lauf_asm_chunk*        chunk = nullptr;

    lauf::array_list<lauf_asm_block> blocks;
    lauf_asm_block*                  cur = nullptr;

    lauf::array_list<lauf_asm_local> locals;
    std::uint16_t                    local_allocation_size = 0;
    bool                             errored               = false;

    explicit lauf_asm_builder(lauf::arena_key key, lauf_asm_build_options options)
    : lauf::intrinsic_arena<lauf_asm_builder>(key), options(options)
    {}

    void error(const char* context, const char* msg);

    void reset(lauf_asm_module* mod, lauf_asm_function* fn, lauf_asm_chunk* chunk)
    {
        this->mod   = mod;
        this->fn    = fn;
        this->chunk = chunk;

        blocks.reset();
        cur = nullptr;

        locals.reset();
        local_allocation_size = 0;
        errored               = false;

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

#define LAUF_BUILD_CHECK_CUR                                                                       \
    do                                                                                             \
    {                                                                                              \
        if (b->cur == nullptr)                                                                     \
            return;                                                                                \
    } while (0)

//=== instruction building ===//
#define LAUF_BUILD_INST_NONE(Name)                                                                 \
    [&] {                                                                                          \
        lauf_asm_inst result;                                                                      \
        result.Name = {lauf::asm_op::Name};                                                        \
        return result;                                                                             \
    }()

#define LAUF_BUILD_INST_OFFSET(Name, Offset)                                                       \
    [&](const char* context, std::ptrdiff_t offset) {                                              \
        lauf_asm_inst result;                                                                      \
        result.Name = {lauf::asm_op::Name, std::int32_t(offset)};                                  \
        if (result.Name.offset != offset)                                                          \
            b->error(context, "offset too big");                                                   \
        return result;                                                                             \
    }(LAUF_BUILD_ASSERT_CONTEXT, static_cast<std::int64_t>(Offset))

#define LAUF_BUILD_INST_STACK_IDX(Name, Idx)                                                       \
    [&] {                                                                                          \
        lauf_asm_inst result;                                                                      \
        result.Name = {lauf::asm_op::Name, Idx};                                                   \
        return result;                                                                             \
    }()

#define LAUF_BUILD_INST_SIGNATURE(Name, InputCount, OutputCount, Flags)                            \
    [&] {                                                                                          \
        lauf_asm_inst result;                                                                      \
        result.Name = {lauf::asm_op::Name, InputCount, OutputCount, std::uint8_t(Flags)};          \
        return result;                                                                             \
    }()

#define LAUF_BUILD_INST_LAYOUT(Name, Layout)                                                       \
    [&](const char* context, lauf_asm_layout layout) {                                             \
        lauf_asm_inst result;                                                                      \
        result.Name = {lauf::asm_op::Name, lauf::align_log2(layout.alignment),                     \
                       std::uint16_t(layout.size)};                                                \
        if (result.Name.size != layout.size)                                                       \
            b->error(context, "size too big");                                                     \
        return result;                                                                             \
    }(LAUF_BUILD_ASSERT_CONTEXT, Layout)

#define LAUF_BUILD_INST_VALUE(Name, Value)                                                         \
    [&](const char* context, std::size_t value) {                                                  \
        lauf_asm_inst result;                                                                      \
        result.Name = {lauf::asm_op::Name, std::uint32_t(value)};                                  \
        if (value != result.Name.value)                                                            \
            b->error(context, "invalid value");                                                    \
        return result;                                                                             \
    }(LAUF_BUILD_ASSERT_CONTEXT, Value)

#define LAUF_BUILD_INST_LOCAL_ADDR(Name, Index, Offset)                                            \
    [&](const char* context, std::size_t index) {                                                  \
        lauf_asm_inst result;                                                                      \
        result.Name = {lauf::asm_op::Name, std::uint8_t(index), Offset};                           \
        if (index != result.Name.index)                                                            \
            b->error(context, "too many local variables");                                         \
        return result;                                                                             \
    }(LAUF_BUILD_ASSERT_CONTEXT, Index)

#endif // SRC_LAUF_ASM_BUILDER_HPP_INCLUDED

