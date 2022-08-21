// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib/memory.h>

#include <lauf/asm/type.h>
#include <lauf/runtime/builtin.h>
#include <lauf/runtime/process.h>
#include <lauf/runtime/value.h>

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_poison, 1, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY, "poison", nullptr)
{
    auto address = vstack_ptr[0].as_address;
    ++vstack_ptr;

    if (!lauf_runtime_poison_allocation(process, address))
        return lauf_runtime_panic(process, "invalid address");

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_unpoison, 1, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY, "unpoison",
                     &lauf_lib_memory_poison)
{
    auto address = vstack_ptr[0].as_address;
    ++vstack_ptr;

    if (!lauf_runtime_unpoison_allocation(process, address))
        return lauf_runtime_panic(process, "invalid address");

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_split, 1, 2, LAUF_RUNTIME_BUILTIN_VM_ONLY, "split",
                     &lauf_lib_memory_unpoison)
{
    auto addr = vstack_ptr[0].as_address;

    --vstack_ptr;
    if (!lauf_runtime_split_allocation(process, addr, &vstack_ptr[1].as_address,
                                       &vstack_ptr[0].as_address))
        return lauf_runtime_panic(process, "invalid address");

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_merge, 2, 1, LAUF_RUNTIME_BUILTIN_VM_ONLY, "merge",
                     &lauf_lib_memory_split)
{
    auto addr1 = vstack_ptr[1].as_address;
    auto addr2 = vstack_ptr[0].as_address;
    ++vstack_ptr;

    if (!lauf_runtime_merge_allocation(process, addr1, addr2))
        return lauf_runtime_panic(process, "invalid address");

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_addr_to_int, 1, 2, LAUF_RUNTIME_BUILTIN_DEFAULT, "addr_to_int",
                     &lauf_lib_memory_merge)
{
    auto addr = vstack_ptr[0].as_address;

    auto ptr = lauf_runtime_get_const_ptr(process, addr, {0, 1});
    if (ptr == nullptr)
        return lauf_runtime_panic(process, "invalid address");
    auto provenance = lauf_runtime_address{addr.allocation, addr.generation, 0};

    --vstack_ptr;
    vstack_ptr[1].as_address = provenance;
    vstack_ptr[0].as_uint    = reinterpret_cast<std::uintptr_t>(ptr);
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_int_to_addr, 2, 1, LAUF_RUNTIME_BUILTIN_DEFAULT, "int_to_addr",
                     &lauf_lib_memory_addr_to_int)
{
    auto provenance = vstack_ptr[1].as_address;
    auto ptr        = reinterpret_cast<unsigned char*>(vstack_ptr[0].as_uint); // NOLINT

    if (!lauf_runtime_get_address(process, &provenance, ptr))
        return lauf_runtime_panic(process, "invalid int for int_to_addr");

    ++vstack_ptr;
    vstack_ptr[0].as_address = provenance;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_addr_add, 2, 1, LAUF_RUNTIME_BUILTIN_NO_PANIC, "addr_add",
                     &lauf_lib_memory_int_to_addr)
{
    auto addr   = vstack_ptr[1].as_address;
    auto offset = vstack_ptr[0].as_sint;

    lauf_sint result;
    auto      overflow = __builtin_add_overflow(lauf_sint(addr.offset), offset, &result);
    if (overflow || result < 0 || result > UINT32_MAX)
        result = UINT32_MAX;

    ++vstack_ptr;
    vstack_ptr[0].as_address = {addr.allocation, addr.generation, std::uint32_t(result)};
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_addr_sub, 2, 1, LAUF_RUNTIME_BUILTIN_NO_PANIC, "addr_sub",
                     &lauf_lib_memory_addr_add)
{
    auto addr   = vstack_ptr[1].as_address;
    auto offset = vstack_ptr[0].as_sint;

    lauf_sint result;
    auto      overflow = __builtin_sub_overflow(lauf_sint(addr.offset), offset, &result);
    if (overflow || result < 0 || result > UINT32_MAX)
        result = UINT32_MAX;

    ++vstack_ptr;
    vstack_ptr[0].as_address = {addr.allocation, addr.generation, std::uint32_t(result)};
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_addr_distance, 2, 1, LAUF_RUNTIME_BUILTIN_DEFAULT,
                     "addr_distance", &lauf_lib_memory_addr_sub)
{
    auto lhs = vstack_ptr[1].as_address;
    auto rhs = vstack_ptr[0].as_address;

    if (lhs.allocation != rhs.allocation || lhs.generation != rhs.generation)
        return lauf_runtime_panic(process, "addresses are from different allocations");

    auto distance = lauf_sint(lhs.offset) - lauf_sint(rhs.offset);

    ++vstack_ptr;
    vstack_ptr[0].as_sint = distance;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

const lauf_runtime_builtin_library lauf_lib_memory
    = {"lauf.memory", &lauf_lib_memory_addr_distance};

