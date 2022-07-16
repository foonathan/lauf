// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_ASM_MODULE_HPP_INCLUDED
#define SRC_LAUF_ASM_MODULE_HPP_INCLUDED

#include <lauf/asm/module.h>
#include <lauf/support/arena.hpp>

struct lauf_asm_module : lauf::arena<lauf_asm_module>
{
    const char*        name;
    lauf_asm_global*   globals   = nullptr;
    lauf_asm_function* functions = nullptr;

    lauf_asm_module(const char* name) : name(this->strdup(name)) {}
};

struct lauf_asm_global
{
    lauf_asm_global* next;

    const unsigned char* memory;
    std::uint64_t        size : 63;
    enum permissions
    {
        read_only,
        read_write,
    } perms : 1;

    explicit lauf_asm_global(lauf_asm_module* mod)
    : next(mod->globals), memory(nullptr), size(0), perms(read_only)
    {
        mod->globals = this;
    }

    explicit lauf_asm_global(lauf_asm_module* mod, std::size_t size)
    : next(mod->globals), memory(nullptr), size(size), perms(read_write)
    {
        mod->globals = this;
    }

    explicit lauf_asm_global(lauf_asm_module* mod, const void* memory, std::size_t size,
                             permissions p)
    : next(mod->globals), memory(mod->memdup(memory, size)), size(size), perms(p)
    {
        mod->globals = this;
    }
};

struct lauf_asm_function
{
    lauf_asm_function* next;

    const char*        name;
    lauf_asm_signature sig;

    explicit lauf_asm_function(lauf_asm_module* mod, const char* name, lauf_asm_signature sig)
    : next(mod->functions), name(mod->strdup(name)), sig(sig)
    {
        mod->functions = this;
    }
};

#endif // SRC_LAUF_ASM_MODULE_HPP_INCLUDED

