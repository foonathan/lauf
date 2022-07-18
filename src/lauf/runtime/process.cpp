// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/runtime/process.hpp>

#include <lauf/asm/module.hpp>
#include <lauf/asm/type.h>
#include <lauf/runtime/value.h>
#include <lauf/vm.hpp>

// lauf_runtime_get_stacktrace() implemented in stacktrace.cpp

bool lauf_runtime_panic(lauf_runtime_process* p, const char* msg)
{
    p->vm->panic_handler(p, msg);
    return false;
}

namespace
{
const void* checked_offset(lauf::allocation alloc, lauf_runtime_address addr,
                           lauf_asm_layout layout)
{
    if (!lauf::is_usable(alloc.status) || (alloc.generation & 0b11) != addr.generation)
        return nullptr;

    if (addr.offset + layout.size >= alloc.size)
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

