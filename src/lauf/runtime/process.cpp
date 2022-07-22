// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/runtime/process.hpp>

#include <lauf/asm/module.hpp>
#include <lauf/asm/program.hpp>
#include <lauf/asm/type.h>
#include <lauf/runtime/value.h>
#include <lauf/vm.hpp>

const lauf_asm_program* lauf_runtime_get_program(lauf_runtime_process* p)
{
    return p->program;
}

const lauf_runtime_value* lauf_runtime_get_vstack_base(lauf_runtime_process* p)
{
    return p->vm->vstack_base;
}

// lauf_runtime_get_stacktrace() implemented in stacktrace.cpp

// lauf_runtime_call(), lauf_runtime_panic() implemented in vm.cpp

namespace
{
const void* checked_offset(lauf::allocation alloc, lauf_runtime_address addr,
                           lauf_asm_layout layout)
{
    if (!lauf::is_usable(alloc.status) || (alloc.generation & 0b11) != addr.generation)
        return nullptr;

    if (addr.offset + layout.size > alloc.size)
        return nullptr;

    auto ptr = alloc.unchecked_offset(addr.offset);
    if (!lauf::is_aligned(ptr, layout.alignment))
        return nullptr;

    return ptr;
}

const void* checked_offset(lauf::allocation alloc, lauf_runtime_address addr)
{
    if (!lauf::is_usable(alloc.status) || (alloc.generation & 0b11) != addr.generation)
        return nullptr;

    if (addr.offset >= alloc.size)
        return nullptr;

    return alloc.unchecked_offset(addr.offset);
}
} // namespace

const void* lauf_runtime_get_const_ptr(lauf_runtime_process* p, lauf_runtime_address addr,
                                       lauf_asm_layout layout)
{
    if (auto alloc = p->get_allocation(addr.allocation))
        return checked_offset(*alloc, addr, layout);
    else
        return nullptr;
}

void* lauf_runtime_get_mut_ptr(lauf_runtime_process* p, lauf_runtime_address addr,
                               lauf_asm_layout layout)
{
    if (auto alloc = p->get_allocation(addr.allocation);
        alloc != nullptr && !lauf::is_const(alloc->source))
        return const_cast<void*>(checked_offset(*alloc, addr, layout));
    else
        return nullptr;
}

const char* lauf_runtime_get_cstr(lauf_runtime_process* p, lauf_runtime_address addr)
{
    if (auto alloc = p->get_allocation(addr.allocation))
    {
        auto str = static_cast<const char*>(checked_offset(*alloc, addr));
        if (str == nullptr)
            return nullptr;

        for (auto cur = str; cur < alloc->unchecked_offset(alloc->size); ++cur)
            if (*cur == '\0')
                // We found a zero byte within the alloction, so it's a C string.
                return str;

        // Did not find the null terminator.
        return nullptr;
    }

    return nullptr;
}

const lauf_asm_function* lauf_runtime_get_function_ptr_any(lauf_runtime_process*         p,
                                                           lauf_runtime_function_address addr)
{
    if (addr.index < p->program->functions.size())
        return p->program->functions[addr.index];
    else
        return nullptr;
}

const lauf_asm_function* lauf_runtime_get_function_ptr(lauf_runtime_process*         p,
                                                       lauf_runtime_function_address addr,
                                                       lauf_asm_signature            signature)
{
    if (addr.input_count != signature.input_count || addr.output_count != signature.output_count)
        return nullptr;
    else
        return lauf_runtime_get_function_ptr_any(p, addr);
}

