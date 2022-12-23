// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_ASM_MODULE_HPP_INCLUDED
#define SRC_LAUF_ASM_MODULE_HPP_INCLUDED

#include <lauf/asm/instruction.hpp>
#include <lauf/asm/module.h>
#include <lauf/support/arena.hpp>
#include <lauf/support/array_list.hpp>

namespace lauf
{
struct inst_debug_location
{
    std::uint16_t           function_idx;
    std::uint16_t           inst_idx;
    lauf_asm_debug_location location;
};
} // namespace lauf

struct lauf_asm_module : lauf::intrinsic_arena<lauf_asm_module>
{
    const char*        name;
    lauf_asm_global*   globals         = nullptr;
    lauf_asm_function* functions       = nullptr;
    lauf_asm_chunk*    chunks          = nullptr;
    std::uint32_t      globals_count   = 0;
    std::uint32_t      functions_count = 0;

    const char*                                 debug_path = nullptr;
    lauf::array_list<lauf::inst_debug_location> inst_debug_locations;

    lauf_asm_module(lauf::arena_key key, const char* name)
    : lauf::intrinsic_arena<lauf_asm_module>(key), name(this->strdup(name))
    {}

    ~lauf_asm_module();
};

struct lauf_asm_global
{
    lauf_asm_global* next;

    const unsigned char* memory;
    std::uint64_t        size : 64;

    std::uint32_t allocation_idx;
    std::uint16_t alignment;
    bool          is_mutable;

    const char* name = nullptr;

    explicit lauf_asm_global(lauf_asm_module* mod, bool is_mutable)
    : next(mod->globals), memory(nullptr), size(0), allocation_idx(mod->globals_count),
      alignment(alignof(lauf_uint)), is_mutable(is_mutable)
    {
        mod->globals = this;
        ++mod->globals_count;
    }

    bool has_definition() const
    {
        return size != 0;
    }
};

struct lauf_asm_function
{
    lauf_asm_function* next;
    lauf_asm_module*   module;

    const char*        name;
    lauf_asm_signature sig;
    bool               exported = false;

    lauf_asm_inst* insts           = nullptr;
    std::uint16_t  inst_count      = 0;
    std::uint16_t  function_idx    = UINT16_MAX;
    std::uint16_t  max_vstack_size = 0;
    // Includes size for stack frame as well.
    std::uint16_t max_cstack_size = 0;

    explicit lauf_asm_function(lauf_asm_module* mod, const char* name, lauf_asm_signature sig)
    : next(mod->functions), module(mod), name(mod->strdup(name)), sig(sig),
      function_idx(std::uint16_t(mod->functions_count))
    {
        mod->functions = this;
        ++mod->functions_count;
    }

    explicit lauf_asm_function(lauf_asm_chunk*, lauf_asm_module* mod, const char* name,
                               lauf_asm_signature sig)
    : next(nullptr), module(mod), name(name), sig(sig)
    {}
};

struct lauf_asm_chunk : lauf::intrinsic_arena<lauf_asm_chunk>
{
    lauf_asm_chunk* next;

    // A chunk is internally just a function, but the memory for the instruction is allocated by its
    // arena, not as part of the module.
    lauf_asm_function* fn;

    // Since a chunk is temporary, we can't store the debug locations in the module.
    lauf::array_list<lauf::inst_debug_location> inst_debug_locations;

    explicit lauf_asm_chunk(lauf::arena_key key, lauf_asm_module* mod)
    : lauf::intrinsic_arena<lauf_asm_chunk>(key), next(mod->chunks),
      // We allocate the function as part of the module, to ensure that its address is closer to the
      // addresses of the functions it calls. This is required by the offsets.
      fn(mod->construct<lauf_asm_function>(this, mod, "<chunk>", lauf_asm_signature{0, 0}))
    {
        mod->chunks = this;
    }

    void reset()
    {
        clear();
        inst_debug_locations.reset();

        *fn = lauf_asm_function(this, fn->module, "<chunk>", lauf_asm_signature{0, 0});
    }
};

#endif // SRC_LAUF_ASM_MODULE_HPP_INCLUDED

