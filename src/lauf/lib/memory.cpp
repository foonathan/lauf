// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib/memory.h>

#include <cstring>
#include <lauf/asm/type.h>
#include <lauf/runtime/builtin.h>
#include <lauf/runtime/memory.h>
#include <lauf/runtime/process.h>
#include <lauf/runtime/value.h>

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_poison, 1, 0, LAUF_RUNTIME_BUILTIN_VM_DIRECTIVE, "poison",
                     nullptr)
{
    auto address = vstack_ptr[0].as_address;
    ++vstack_ptr;

    if (!lauf_runtime_poison_allocation(process, address))
        return lauf_runtime_panic(process, "invalid address");

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_unpoison, 1, 0, LAUF_RUNTIME_BUILTIN_VM_DIRECTIVE, "unpoison",
                     &lauf_lib_memory_poison)
{
    auto address = vstack_ptr[0].as_address;
    ++vstack_ptr;

    if (!lauf_runtime_unpoison_allocation(process, address))
        return lauf_runtime_panic(process, "invalid address");

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_split, 1, 2, LAUF_RUNTIME_BUILTIN_DEFAULT, "split",
                     &lauf_lib_memory_unpoison)
{
    auto addr = vstack_ptr[0].as_address;

    --vstack_ptr;
    if (!lauf_runtime_split_allocation(process, addr, &vstack_ptr[1].as_address,
                                       &vstack_ptr[0].as_address))
        return lauf_runtime_panic(process, "invalid address");

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_merge, 2, 1, LAUF_RUNTIME_BUILTIN_DEFAULT, "merge",
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

namespace
{
std::uint32_t addr_offset(lauf_runtime_address addr, lauf_sint offset)
{
    lauf_sint result;
    auto      overflow = __builtin_add_overflow(lauf_sint(addr.offset), offset, &result);
    if (LAUF_UNLIKELY(overflow || result < 0 || result > UINT32_MAX))
        result = UINT32_MAX;

    return std::uint32_t(result);
}

template <bool Strict>
bool validate_addr_offset(lauf_runtime_process* process, lauf_runtime_address addr,
                          std::uint32_t new_offset)
{
    lauf_runtime_allocation alloc;
    if (!lauf_runtime_get_allocation(process, addr, &alloc))
        return lauf_runtime_panic(process, "invalid address");

    if ((Strict && new_offset >= alloc.size) || (!Strict && new_offset > alloc.size))
        return lauf_runtime_panic(process, "address overflow");

    return true;
}

LAUF_RUNTIME_BUILTIN(addr_add_invalidate, 2, 1, LAUF_RUNTIME_BUILTIN_NO_PANIC,
                     "addr_add_invalidate", &lauf_lib_memory_int_to_addr)
{
    auto addr   = vstack_ptr[1].as_address;
    auto offset = vstack_ptr[0].as_sint;

    auto new_offset = addr_offset(addr, offset);

    ++vstack_ptr;
    vstack_ptr[0].as_address = {addr.allocation, addr.generation, new_offset};
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(addr_add_panic, 2, 1, LAUF_RUNTIME_BUILTIN_DEFAULT, "addr_add_panic",
                     &addr_add_invalidate)
{
    auto addr   = vstack_ptr[1].as_address;
    auto offset = vstack_ptr[0].as_sint;

    auto new_offset = addr_offset(addr, offset);
    if (!validate_addr_offset<false>(process, addr, new_offset))
        return false;

    ++vstack_ptr;
    vstack_ptr[0].as_address = {addr.allocation, addr.generation, new_offset};
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(addr_add_panic_strict, 2, 1, LAUF_RUNTIME_BUILTIN_DEFAULT,
                     "addr_add_panic_strict", &addr_add_panic)
{
    auto addr   = vstack_ptr[1].as_address;
    auto offset = vstack_ptr[0].as_sint;

    auto new_offset = addr_offset(addr, offset);
    if (!validate_addr_offset<true>(process, addr, new_offset))
        return false;

    ++vstack_ptr;
    vstack_ptr[0].as_address = {addr.allocation, addr.generation, new_offset};
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(addr_sub_invalidate, 2, 1, LAUF_RUNTIME_BUILTIN_NO_PANIC,
                     "addr_sub_invalidate", &addr_add_panic_strict)
{
    auto addr   = vstack_ptr[1].as_address;
    auto offset = vstack_ptr[0].as_sint;

    auto new_offset = addr_offset(addr, -offset);

    ++vstack_ptr;
    vstack_ptr[0].as_address = {addr.allocation, addr.generation, new_offset};
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(addr_sub_panic, 2, 1, LAUF_RUNTIME_BUILTIN_DEFAULT, "addr_sub_panic",
                     &addr_sub_invalidate)
{
    auto addr   = vstack_ptr[1].as_address;
    auto offset = vstack_ptr[0].as_sint;

    auto new_offset = addr_offset(addr, -offset);
    if (!validate_addr_offset<false>(process, addr, new_offset))
        return false;

    ++vstack_ptr;
    vstack_ptr[0].as_address = {addr.allocation, addr.generation, new_offset};
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
LAUF_RUNTIME_BUILTIN(addr_sub_panic_strict, 2, 1, LAUF_RUNTIME_BUILTIN_DEFAULT,
                     "addr_sub_panic_strict", &addr_sub_panic)
{
    auto addr   = vstack_ptr[1].as_address;
    auto offset = vstack_ptr[0].as_sint;

    auto new_offset = addr_offset(addr, -offset);
    if (!validate_addr_offset<true>(process, addr, new_offset))
        return false;

    ++vstack_ptr;
    vstack_ptr[0].as_address = {addr.allocation, addr.generation, new_offset};
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
} // namespace

lauf_runtime_builtin lauf_lib_memory_addr_add(lauf_lib_memory_addr_overflow overflow)
{
    switch (overflow)
    {
    case LAUF_LIB_MEMORY_ADDR_OVERFLOW_INVALIDATE:
        return addr_add_invalidate;
    case LAUF_LIB_MEMORY_ADDR_OVERFLOW_PANIC:
        return addr_add_panic;
    case LAUF_LIB_MEMORY_ADDR_OVERFLOW_PANIC_STRICT:
        return addr_add_panic_strict;
    }
}

lauf_runtime_builtin lauf_lib_memory_addr_sub(lauf_lib_memory_addr_overflow overflow)
{
    switch (overflow)
    {
    case LAUF_LIB_MEMORY_ADDR_OVERFLOW_INVALIDATE:
        return addr_sub_invalidate;
    case LAUF_LIB_MEMORY_ADDR_OVERFLOW_PANIC:
        return addr_sub_panic;
    case LAUF_LIB_MEMORY_ADDR_OVERFLOW_PANIC_STRICT:
        return addr_sub_panic_strict;
    }
}

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_addr_distance, 2, 1, LAUF_RUNTIME_BUILTIN_DEFAULT,
                     "addr_distance", &addr_sub_panic_strict)
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

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_copy, 3, 0, LAUF_RUNTIME_BUILTIN_DEFAULT, "copy",
                     &lauf_lib_memory_addr_distance)
{
    auto dest  = vstack_ptr[2].as_address;
    auto src   = vstack_ptr[1].as_address;
    auto count = vstack_ptr[0].as_uint;

    auto dest_ptr = lauf_runtime_get_mut_ptr(process, dest, {count, 1});
    auto src_ptr  = lauf_runtime_get_const_ptr(process, src, {count, 1});
    if (dest_ptr == nullptr || src_ptr == nullptr)
        return lauf_runtime_panic(process, "invalid address");

    std::memmove(dest_ptr, src_ptr, count);

    vstack_ptr += 3;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_fill, 3, 0, LAUF_RUNTIME_BUILTIN_DEFAULT, "fill",
                     &lauf_lib_memory_copy)
{
    auto dest  = vstack_ptr[2].as_address;
    auto byte  = vstack_ptr[1].as_uint;
    auto count = vstack_ptr[0].as_uint;

    auto dest_ptr = lauf_runtime_get_mut_ptr(process, dest, {count, 1});
    if (dest_ptr == nullptr)
        return lauf_runtime_panic(process, "invalid address");

    std::memset(dest_ptr, static_cast<unsigned char>(byte), count);

    vstack_ptr += 3;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_memory_cmp, 3, 1, LAUF_RUNTIME_BUILTIN_DEFAULT, "cmp",
                     &lauf_lib_memory_fill)
{
    auto lhs   = vstack_ptr[2].as_address;
    auto rhs   = vstack_ptr[1].as_address;
    auto count = vstack_ptr[0].as_uint;

    auto lhs_ptr = lauf_runtime_get_const_ptr(process, lhs, {count, 1});
    auto rhs_ptr = lauf_runtime_get_const_ptr(process, rhs, {count, 1});
    if (lhs_ptr == nullptr || rhs_ptr == nullptr)
        return lauf_runtime_panic(process, "invalid address");

    auto result = std::memcmp(lhs_ptr, rhs_ptr, count);

    vstack_ptr += 2;
    vstack_ptr[0].as_sint = result;
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

const lauf_runtime_builtin_library lauf_lib_memory = {"lauf.memory", &lauf_lib_memory_cmp, nullptr};

