// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_ASM_BUILDER_HPP_INCLUDED
#define SRC_LAUF_ASM_BUILDER_HPP_INCLUDED

#include <deque>
#include <lauf/asm/builder.h>
#include <lauf/asm/instruction.hpp>
#include <lauf/asm/module.h>
#include <vector>

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

struct lauf_asm_block
{
    lauf_asm_signature        sig;
    lauf::vstack_size_checker vstack;

    std::ptrdiff_t              offset;
    std::vector<lauf::asm_inst> insts;

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

struct lauf_asm_builder
{
    lauf_asm_build_options options;
    lauf_asm_module*       mod = nullptr;
    lauf_asm_function*     fn  = nullptr;

    std::deque<lauf_asm_block> blocks;
    lauf_asm_block*            cur = nullptr;

    bool errored = false;

    explicit lauf_asm_builder(lauf_asm_build_options options) : options(options) {}

    void error(const char* context, const char* msg);

    void reset(lauf_asm_module* mod, lauf_asm_function* fn)
    {
        this->mod = mod;
        this->fn  = fn;

        blocks.clear();
        cur = nullptr;

        errored = false;
    }
};

#endif // SRC_LAUF_ASM_BUILDER_HPP_INCLUDED

