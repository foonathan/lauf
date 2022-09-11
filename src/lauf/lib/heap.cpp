// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib/heap.h>

#include <cstring>
#include <lauf/runtime/builtin.h>
#include <lauf/runtime/memory.h>
#include <lauf/runtime/process.h>
#include <lauf/runtime/value.h>
#include <lauf/support/align.hpp>
#include <lauf/vm.h>

LAUF_RUNTIME_BUILTIN(lauf_lib_heap_alloc, 2, 1, LAUF_RUNTIME_BUILTIN_VM_ONLY, "alloc", nullptr)
{
    auto size      = vstack_ptr[0].as_uint;
    auto alignment = vstack_ptr[1].as_uint;

    auto allocator = lauf_vm_get_allocator(lauf_runtime_get_vm(process));
    auto memory    = allocator.heap_alloc(allocator.user_data, size, alignment);
    if (memory == nullptr)
        return lauf_runtime_panic(process, "out of memory");

    auto address = lauf_runtime_add_heap_allocation(process, memory, size);

    ++vstack_ptr;
    vstack_ptr[0].as_address = address;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_heap_alloc_array, 3, 1, LAUF_RUNTIME_BUILTIN_VM_ONLY, "alloc_array",
                     &lauf_lib_heap_alloc)
{
    auto count     = vstack_ptr[0].as_uint;
    auto size      = vstack_ptr[1].as_uint;
    auto alignment = vstack_ptr[2].as_uint;

    auto memory_size = lauf::round_to_multiple_of_alignment(size, alignment) * count;

    ++vstack_ptr;
    vstack_ptr[0].as_uint = memory_size;

    LAUF_TAIL_CALL return lauf_lib_heap_alloc.impl(ip, vstack_ptr, frame_ptr, process);
}

LAUF_RUNTIME_BUILTIN(lauf_lib_heap_free, 1, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY, "free",
                     &lauf_lib_heap_alloc_array)
{
    auto address = vstack_ptr[0].as_address;
    ++vstack_ptr;

    lauf_runtime_allocation alloc;
    if (!lauf_runtime_get_allocation(process, address, &alloc)
        || !lauf_runtime_leak_heap_allocation(process, address))
        return lauf_runtime_panic(process, "invalid heap address");

    auto allocator = lauf_vm_get_allocator(lauf_runtime_get_vm(process));
    allocator.free_alloc(allocator.user_data, alloc.ptr, alloc.size);

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_heap_leak, 1, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY, "leak",
                     &lauf_lib_heap_free)
{
    auto address = vstack_ptr[0].as_address;
    ++vstack_ptr;

    if (!lauf_runtime_leak_heap_allocation(process, address))
        return lauf_runtime_panic(process, "invalid heap address");

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_heap_transfer_local, 1, 1, LAUF_RUNTIME_BUILTIN_VM_ONLY,
                     "transfer_local", &lauf_lib_heap_leak)
{
    auto address = vstack_ptr[0].as_address;

    lauf_runtime_allocation alloc;
    if (!lauf_runtime_get_allocation(process, address, &alloc)
        || (alloc.permission & LAUF_RUNTIME_PERM_READ) == 0)
        return lauf_runtime_panic(process, "invalid address");

    if (alloc.source == LAUF_RUNTIME_LOCAL_ALLOCATION)
    {
        auto allocator = lauf_vm_get_allocator(lauf_runtime_get_vm(process));
        auto memory    = allocator.heap_alloc(allocator.user_data, alloc.size, alignof(void*));
        if (memory == nullptr)
            return lauf_runtime_panic(process, "out of memory");

        std::memcpy(memory, alloc.ptr, alloc.size);

        vstack_ptr[0].as_address = lauf_runtime_add_heap_allocation(process, memory, alloc.size);
    }

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_heap_gc, 0, 1, LAUF_RUNTIME_BUILTIN_VM_ONLY, "gc",
                     &lauf_lib_heap_transfer_local)
{
    auto bytes_freed = lauf_runtime_gc(process);

    --vstack_ptr;
    vstack_ptr[0].as_uint = bytes_freed;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_heap_declare_reachable, 1, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY,
                     "declare_reachable", &lauf_lib_heap_gc)
{
    auto addr = vstack_ptr[0].as_address;
    ++vstack_ptr;

    if (!lauf_runtime_declare_reachable(process, addr))
        return lauf_runtime_panic(process, "invalid heap address");

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_heap_undeclare_reachable, 1, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY,
                     "undeclare_reachable", &lauf_lib_heap_declare_reachable)
{
    auto addr = vstack_ptr[0].as_address;
    ++vstack_ptr;

    if (!lauf_runtime_undeclare_reachable(process, addr))
        return lauf_runtime_panic(process, "invalid heap address");

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_heap_declare_weak, 1, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY, "declare_weak",
                     &lauf_lib_heap_undeclare_reachable)
{
    auto addr = vstack_ptr[0].as_address;
    ++vstack_ptr;

    if (!lauf_runtime_declare_weak(process, addr))
        return lauf_runtime_panic(process, "invalid address");

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_heap_undeclare_weak, 1, 0, LAUF_RUNTIME_BUILTIN_VM_ONLY,
                     "undeclare_weak", &lauf_lib_heap_declare_weak)
{
    auto addr = vstack_ptr[0].as_address;
    ++vstack_ptr;

    if (!lauf_runtime_undeclare_weak(process, addr))
        return lauf_runtime_panic(process, "invalid address");

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

const lauf_runtime_builtin_library lauf_lib_heap
    = {"lauf.heap", &lauf_lib_heap_undeclare_weak, nullptr};

