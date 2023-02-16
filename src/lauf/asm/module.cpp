// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/asm/module.hpp>

#include <lauf/asm/type.h>
#include <string_view>

const lauf_asm_debug_location lauf_asm_debug_location_null = {uint16_t(-1), 0, 0, false, 0};

bool lauf_asm_debug_location_eq(lauf_asm_debug_location lhs, lauf_asm_debug_location rhs)
{
    return lhs.file_id == rhs.file_id && lhs.line_nr == rhs.line_nr
           && lhs.column_nr == rhs.column_nr && lhs.is_synthetic == rhs.is_synthetic
           && lhs.length == rhs.length;
}

lauf_asm_module::~lauf_asm_module()
{
    auto chunk = chunks;
    while (chunk != nullptr)
    {
        auto next = chunk->next;
        lauf_asm_chunk::destroy(chunk);
        chunk = next;
    }
}

lauf_asm_module* lauf_asm_create_module(const char* name)
{
    return lauf_asm_module::create(name);
}

void lauf_asm_destroy_module(lauf_asm_module* mod)
{
    lauf_asm_module::destroy(mod);
}

void lauf_asm_set_module_debug_path(lauf_asm_module* mod, const char* path)
{
    mod->debug_path = mod->strdup(path);
}

const char* lauf_asm_module_debug_path(lauf_asm_module* mod)
{
    return mod->debug_path;
}

const lauf_asm_function* lauf_asm_find_function_by_name(const lauf_asm_module* mod,
                                                        const char*            _name)
{
    auto name = std::string_view(_name);
    for (auto fn = mod->functions; fn != nullptr; fn = fn->next)
        if (fn->name == name)
            return fn;

    return nullptr;
}

const lauf_asm_function* lauf_asm_find_function_of_instruction(const lauf_asm_module* mod,
                                                               const lauf_asm_inst*   ip)
{
    for (auto fn = mod->functions; fn != nullptr; fn = fn->next)
        if (ip >= fn->insts && ip < fn->insts + fn->inst_count)
            return fn;

    return nullptr;
}

const lauf_asm_chunk* lauf_asm_find_chunk_of_instruction(const lauf_asm_module* mod,
                                                         const lauf_asm_inst*   ip)
{
    for (auto chunk = mod->chunks; chunk != nullptr; chunk = chunk->next)
        if (ip >= chunk->fn->insts && ip < chunk->fn->insts + chunk->fn->inst_count)
            return chunk;

    return nullptr;
}

namespace
{
lauf_asm_debug_location find_debug_location(
    const lauf::array_list<lauf::inst_debug_location>& locations, const lauf_asm_function* fn,
    const lauf_asm_inst* ip)
{
    auto fn_idx = fn->function_idx;
    auto ip_idx = uint16_t(lauf_asm_get_instruction_index(fn, ip));

    auto have_found_fn = false;
    auto result        = lauf_asm_debug_location_null;
    for (auto loc : locations)
    {
        if (loc.function_idx == fn_idx)
        {
            have_found_fn = true;

            if (loc.inst_idx > ip_idx)
                break;

            result = loc.location;
        }
        else if (have_found_fn)
            break;
    }
    return result;
}
} // namespace

lauf_asm_debug_location lauf_asm_find_debug_location_of_instruction(const lauf_asm_module* mod,
                                                                    const lauf_asm_inst*   ip)
{
    if (mod->inst_debug_locations.empty() && mod->chunks == nullptr)
        // Early exit, no debug locations are stored anywhere.
        return lauf_asm_debug_location_null;

    if (auto fn = lauf_asm_find_function_of_instruction(mod, ip))
        return find_debug_location(mod->inst_debug_locations, fn, ip);

    if (auto chunk = lauf_asm_find_chunk_of_instruction(mod, ip))
        return find_debug_location(chunk->inst_debug_locations, chunk->fn, ip);

    // We don't have a debug location for it.
    return lauf_asm_debug_location_null;
}

lauf_asm_global* lauf_asm_add_global(lauf_asm_module* mod, lauf_asm_global_permissions perms)
{
    return mod->construct<lauf_asm_global>(mod, perms == LAUF_ASM_GLOBAL_READ_WRITE);
}

void lauf_asm_define_data_global(lauf_asm_module* mod, lauf_asm_global* global,
                                 lauf_asm_layout layout, const void* data)
{
    assert(layout.size > 0);
    global->size      = layout.size;
    global->alignment = std::uint16_t(layout.alignment);

    if (data != nullptr)
        global->memory = mod->memdup(data, layout.size);
}

void lauf_asm_set_global_debug_name(lauf_asm_module* mod, lauf_asm_global* global, const char* name)
{
    global->name = mod->strdup(name);
}

bool lauf_asm_global_has_definition(const lauf_asm_global* global)
{
    return global->has_definition();
}

lauf_asm_layout lauf_asm_global_layout(const lauf_asm_global* global)
{
    assert(global->has_definition());
    return {global->size, global->alignment};
}

const char* lauf_asm_global_debug_name(const lauf_asm_global* global)
{
    return global->name;
}

lauf_asm_function* lauf_asm_add_function(lauf_asm_module* mod, const char* name,
                                         lauf_asm_signature sig)
{
    return mod->construct<lauf_asm_function>(mod, name, sig);
}

void lauf_asm_export_function(lauf_asm_function* fn)
{
    fn->exported = true;
}

const char* lauf_asm_function_name(const lauf_asm_function* fn)
{
    return fn->name;
}

lauf_asm_signature lauf_asm_function_signature(const lauf_asm_function* fn)
{
    return fn->sig;
}

bool lauf_asm_function_has_definition(const lauf_asm_function* fn)
{
    return fn->insts != nullptr;
}

size_t lauf_asm_get_instruction_index(const lauf_asm_function* fn, const lauf_asm_inst* ip)
{
    assert(ip >= fn->insts && ip < fn->insts + fn->inst_count);
    return size_t(ip - fn->insts);
}

lauf_asm_chunk* lauf_asm_create_chunk(lauf_asm_module* mod)
{
    return lauf_asm_chunk::create(mod);
}

lauf_asm_signature lauf_asm_chunk_signature(const lauf_asm_chunk* chunk)
{
    return chunk->fn->sig;
}

bool lauf_asm_chunk_is_empty(const lauf_asm_chunk* chunk)
{
    return chunk->fn->inst_count == 0;
}

