// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/runtime/memory.hpp>

#include <lauf/asm/module.hpp>
#include <lauf/asm/program.hpp>
#include <lauf/runtime/process.hpp>
#include <lauf/vm.hpp>

namespace
{
lauf::allocation allocate_global(lauf::arena_base& arena, lauf_asm_global global)
{
    lauf::allocation result;

    if (global.memory != nullptr)
    {
        result.ptr = arena.memdup(global.memory, global.size, global.alignment);
    }
    else
    {
        result.ptr = arena.allocate(global.size, global.alignment);
        std::memset(result.ptr, 0, global.size);
    }

    // If bigger than 32bit, only the lower parts are addressable.
    result.size = std::uint32_t(global.size);

    result.source     = global.perms == lauf_asm_global::read_write
                            ? lauf::allocation_source::static_mut_memory
                            : lauf::allocation_source::static_const_memory;
    result.status     = lauf::allocation_status::allocated;
    result.gc         = lauf::gc_tracking::reachable_explicit;
    result.generation = 0;

    return result;
}
} // namespace

void lauf::memory::init(lauf_vm* vm, const lauf_asm_module* mod)
{
    _allocations.resize_uninitialized(vm->page_allocator, mod->globals_count);
    for (auto global = mod->globals; global != nullptr; global = global->next)
        _allocations[global->allocation_idx] = allocate_global(*vm, *global);
}

void lauf::memory::clear(lauf_vm* vm)
{
    _allocations.clear(vm->page_allocator);
}

const void* lauf_runtime_get_const_ptr(lauf_runtime_process* p, lauf_runtime_address addr,
                                       lauf_asm_layout layout)
{
    if (auto alloc = p->memory.try_get(addr))
        return lauf::checked_offset(*alloc, addr, layout);
    else
        return nullptr;
}

void* lauf_runtime_get_mut_ptr(lauf_runtime_process* p, lauf_runtime_address addr,
                               lauf_asm_layout layout)
{
    if (auto alloc = p->memory.try_get(addr); alloc != nullptr && !lauf::is_const(alloc->source))
        return const_cast<void*>(lauf::checked_offset(*alloc, addr, layout));
    else
        return nullptr;
}

bool lauf_runtime_get_address(lauf_runtime_process* p, lauf_runtime_address* allocation,
                              const void* ptr)
{
    auto alloc = p->memory.try_get(*allocation);
    if (alloc == nullptr)
        return false;

    auto offset = static_cast<const unsigned char*>(ptr) - static_cast<unsigned char*>(alloc->ptr);
    if (offset < 0 || offset >= alloc->size)
        return false;

    allocation->offset = std::uint32_t(offset);
    return true;
}

lauf_runtime_address lauf_runtime_get_global_address(lauf_runtime_process*,
                                                     const lauf_asm_global* global)
{
    return {global->allocation_idx, 0, 0};
}

const char* lauf_runtime_get_cstr(lauf_runtime_process* p, lauf_runtime_address addr)
{
    if (auto alloc = p->memory.try_get(addr))
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

lauf_runtime_fiber* lauf_runtime_get_fiber_ptr(lauf_runtime_process* p, lauf_runtime_address addr)
{
    return lauf::get_fiber(p, addr);
}

lauf_runtime_address lauf_runtime_add_heap_allocation(lauf_runtime_process* p, void* ptr,
                                                      size_t size)
{
    auto alloc = lauf::make_heap_alloc(ptr, size, p->memory.cur_generation());
    return p->memory.new_allocation(p->vm->page_allocator, alloc);
}

bool lauf_runtime_get_allocation(lauf_runtime_process* p, lauf_runtime_address addr,
                                 lauf_runtime_allocation* result)
{
    auto alloc = p->memory.try_get(addr);
    if (alloc == nullptr)
        return false;

    result->ptr  = alloc->ptr;
    result->size = alloc->size;

    if (lauf::is_usable(alloc->status))
        result->permission
            = lauf_runtime_permission(LAUF_RUNTIME_PERM_READ | LAUF_RUNTIME_PERM_WRITE);
    else
        result->permission = LAUF_RUNTIME_PERM_NONE;

    switch (alloc->source)
    {
    case lauf::allocation_source::static_const_memory:
        result->permission = lauf_runtime_permission(result->permission & ~LAUF_RUNTIME_PERM_WRITE);
        // fallthrough
    case lauf::allocation_source::static_mut_memory:
        result->source = LAUF_RUNTIME_STATIC_ALLOCATION;
        break;

    case lauf::allocation_source::local_memory:
        result->source = LAUF_RUNTIME_LOCAL_ALLOCATION;
        break;

    case lauf::allocation_source::heap_memory:
        result->source = LAUF_RUNTIME_HEAP_ALLOCATION;
        break;

    case lauf::allocation_source::fiber_memory:
        // We expose it to the user like static memory.
        // That way, they're not tempted to free it.
        // (Note that the garbage collector does free it)
        result->source = LAUF_RUNTIME_STATIC_ALLOCATION;
        break;
    }

    return true;
}

bool lauf_runtime_leak_heap_allocation(lauf_runtime_process* p, lauf_runtime_address addr)
{
    auto alloc = p->memory.try_get(addr);
    if (alloc == nullptr || !lauf::can_be_freed(alloc->status)
        || alloc->source != lauf::allocation_source::heap_memory
        || alloc->split != lauf::allocation_split::unsplit)
        return false;
    alloc->status = lauf::allocation_status::freed;
    return true;
}

size_t lauf_runtime_gc(lauf_runtime_process* p)
{
    auto                                marker = p->vm->get_marker();
    lauf::array_list<lauf::allocation*> pending_allocations;

    auto mark_reachable = [&](lauf::allocation* alloc) {
        if (alloc->status != lauf::allocation_status::freed
            && alloc->gc == lauf::gc_tracking::unreachable)
        {
            // It is a not freed allocation that we didn't know was reachable yet, add it.
            alloc->gc = lauf::gc_tracking::reachable;
            pending_allocations.push_back(*p->vm, alloc);
        }
    };
    auto process_reachable_memory = [&](lauf::allocation* alloc) {
        if (alloc->size < sizeof(lauf_runtime_address) || alloc->is_gc_weak)
            return;

        // Assume the allocation contains an array of values.
        // For that we need to align the pointer properly.
        // (We've done a size check already, so the initial offset is fine)
        auto offset = lauf::align_offset(alloc->ptr, alignof(lauf_runtime_value));
        auto ptr    = reinterpret_cast<lauf_runtime_value*>(static_cast<unsigned char*>(alloc->ptr)
                                                         + offset);

        for (auto end = ptr + (alloc->size - offset) / sizeof(lauf_runtime_value); ptr != end;
             ++ptr)
        {
            auto alloc = p->memory.try_get(ptr->as_address);
            if (alloc != nullptr && ptr->as_address.offset <= alloc->size)
                mark_reachable(alloc);
        }
    };

    // Mark the current fiber as reachable.
    mark_reachable(&p->memory[p->cur_fiber->handle_allocation]);

    // Mark allocations reachable by addresses in the vstack and call stack as reachable.
    // We allow the stacks from all fibers, even unreachable ones.
    // If the fiber in question turned out to be unreaachable, it will be deallocated now,
    // and the next GC run will collect everything that became unreferenced as a consequence of it.
    for (auto fiber = lauf_runtime_iterate_fibers(p); fiber != nullptr;
         fiber      = lauf_runtime_iterate_fibers_next(fiber))
    {
        // Iterate over the vstack.
        for (auto cur = lauf_runtime_get_vstack_ptr(p, fiber);
             cur != lauf_runtime_get_vstack_base(fiber); ++cur)
        {
            auto alloc = p->memory.try_get(cur->as_address);
            if (alloc != nullptr && cur->as_address.offset <= alloc->size)
                mark_reachable(alloc);
        }

        // Iterate over memory in the call stack.
        // This is necessary because we do not generate local allocations if their address is never
        // taken. However, they're still reachable and can contain pointers.
        for (auto frame
             = fiber == p->cur_fiber ? p->regs.frame_ptr : fiber->suspension_point.frame_ptr;
             !frame->is_trampoline_frame(); frame = frame->prev)
        {
            // We create a dummy allocation for the entire stack frame.
            // It is guaranteed to be properly aligned for pointers, so that's not an issue.
            auto alloc
                = lauf::make_local_alloc(frame + 1,
                                         frame->next_offset - sizeof(lauf_runtime_stack_frame),
                                         frame->local_generation);
            process_reachable_memory(&alloc);
        }
    }

    // Process memory from explicitly reachable allocations.
    // This covers local allocations again, but it doesn't matter.
    for (auto& alloc : p->memory)
    {
        // Non-heap non-fiber memory should always be reachable.
        assert(alloc.source == lauf::allocation_source::heap_memory
               || alloc.source == lauf::allocation_source::fiber_memory
               || alloc.gc == lauf::gc_tracking::reachable_explicit);

        if (alloc.gc == lauf::gc_tracking::reachable_explicit
            && alloc.status != lauf::allocation_status::freed)
            process_reachable_memory(&alloc);
    }

    // Recursively mark everything as reachable from reachable allocations.
    while (!pending_allocations.empty())
    {
        auto alloc = pending_allocations.back();
        pending_allocations.pop_back();
        process_reachable_memory(alloc);
    }

    // Free unreachable heap memory.
    auto allocator   = p->vm->heap_allocator;
    auto bytes_freed = std::size_t(0);
    for (auto& alloc : p->memory)
    {
        if (alloc.source == lauf::allocation_source::heap_memory
            && alloc.status != lauf::allocation_status::freed
            && alloc.split == lauf::allocation_split::unsplit
            && alloc.gc == lauf::gc_tracking::unreachable)
        {
            // It is an unreachable allocation that we can free.
            allocator.free_alloc(allocator.user_data, alloc.ptr, alloc.size);
            alloc.status = lauf::allocation_status::freed;
            bytes_freed += alloc.size;
        }
        else if (alloc.source == lauf::allocation_source::fiber_memory
                 && alloc.status != lauf::allocation_status::freed
                 && alloc.gc == lauf::gc_tracking::unreachable)
        {
            assert(alloc.split == lauf::allocation_split::unsplit);

            // It is a fiber we can destroy.
            lauf_runtime_fiber::destroy(p, static_cast<lauf_runtime_fiber*>(alloc.ptr));
            assert(alloc.status == lauf::allocation_status::freed);
        }

        // Need to reset GC tracking for next GC run.
        if (alloc.gc != lauf::gc_tracking::reachable_explicit)
            alloc.gc = lauf::gc_tracking::unreachable;
    }

    p->vm->unwind(marker);
    return bytes_freed;
}

bool lauf_runtime_poison_allocation(lauf_runtime_process* p, lauf_runtime_address addr)
{
    auto alloc = p->memory.try_get(addr);
    if (alloc == nullptr || !lauf::is_usable(alloc->status))
        return false;
    alloc->status = lauf::allocation_status::poison;
    return true;
}

bool lauf_runtime_unpoison_allocation(lauf_runtime_process* p, lauf_runtime_address addr)
{
    auto alloc = p->memory.try_get(addr);
    // Note that we prevent unpoising of fiber memory.
    if (alloc == nullptr || alloc->source == lauf::allocation_source::fiber_memory
        || alloc->status != lauf::allocation_status::poison)
        return false;
    alloc->status = lauf::allocation_status::allocated;
    return true;
}

bool lauf_runtime_split_allocation(lauf_runtime_process* p, lauf_runtime_address addr,
                                   lauf_runtime_address* addr1, lauf_runtime_address* addr2)
{
    auto alloc = p->memory.try_get(addr);
    if (alloc == nullptr || !lauf::is_usable(alloc->status) || addr.offset >= alloc->size)
        return false;

    // We create a new allocation as a copy, but with modified pointer and size.
    // If the original allocation was unsplit or the last split, the new allocation is the end of
    // the allocation. Otherwise, it is somewhere in the middle.
    auto new_alloc = *alloc;
    new_alloc.ptr  = static_cast<unsigned char*>(new_alloc.ptr) + addr.offset;
    new_alloc.size -= addr.offset;
    new_alloc.split = alloc->split == lauf::allocation_split::unsplit
                              || alloc->split == lauf::allocation_split::split_last
                          ? lauf::allocation_split::split_last
                          : lauf::allocation_split::split_middle;
    *addr2          = p->memory.new_allocation(p->vm->page_allocator, new_alloc);

    // We now modify the original allocation, by shrinking it.
    // If the original allocation was unsplit or the first split, it is the first split.
    // Otherwise, it is somewhere in the middle.
    alloc->size  = addr.offset;
    alloc->split = alloc->split == lauf::allocation_split::unsplit
                           || alloc->split == lauf::allocation_split::split_first
                       ? lauf::allocation_split::split_first
                       : lauf::allocation_split::split_middle;
    *addr1       = lauf_runtime_address{addr.allocation, addr.generation, 0};

    return true;
}

bool lauf_runtime_merge_allocation(lauf_runtime_process* p, lauf_runtime_address addr1,
                                   lauf_runtime_address addr2)
{
    auto alloc1 = p->memory.try_get(addr1);
    auto alloc2 = p->memory.try_get(addr2);
    if (alloc1 == nullptr
        || alloc2 == nullptr
        // Allocations must be usable.
        || !lauf::is_usable(alloc1->status)
        || !lauf::is_usable(alloc2->status)
        // Allocations must be split.
        || alloc1->split == lauf::allocation_split::unsplit
        || alloc2->split == lauf::allocation_split::unsplit
        // And they must be adjacent.
        || static_cast<unsigned char*>(alloc1->ptr) + alloc1->size != alloc2->ptr)
        return false;

    // alloc1 grows to cover alloc2.
    alloc1->size += alloc2->size;

    // Since alloc1 precedes alloc2, the following split configuration are possible:
    // 1. (first, mid)
    // 2. (first, last)
    // 3. (mid, mid)
    // 4. (mid, last)
    //
    // In case 2, we merged everything back and are unsplit.
    // In case 4, we create a bigger last split.
    if (alloc2->split == lauf::allocation_split::split_last)
    {
        if (alloc1->split == lauf::allocation_split::split_first)
            alloc1->split = lauf::allocation_split::unsplit; // case 2
        else
            alloc1->split = lauf::allocation_split::split_last; // case 4
    }

    // We don't need alloc2 anymore.
    alloc2->status = lauf::allocation_status::freed;
    return true;
}

bool lauf_runtime_declare_reachable(lauf_runtime_process* p, lauf_runtime_address addr)
{
    auto alloc = p->memory.try_get(addr);
    if (alloc == nullptr || alloc->source != lauf::allocation_source::heap_memory)
        return false;
    alloc->gc = lauf::gc_tracking::reachable_explicit;
    return true;
}

bool lauf_runtime_undeclare_reachable(lauf_runtime_process* p, lauf_runtime_address addr)
{
    auto alloc = p->memory.try_get(addr);
    if (alloc == nullptr || alloc->source != lauf::allocation_source::heap_memory)
        return false;
    alloc->gc = lauf::gc_tracking::unreachable;
    return true;
}

bool lauf_runtime_declare_weak(lauf_runtime_process* p, lauf_runtime_address addr)
{
    auto alloc = p->memory.try_get(addr);
    if (alloc == nullptr)
        return false;
    alloc->is_gc_weak = true;
    return true;
}

bool lauf_runtime_undeclare_weak(lauf_runtime_process* p, lauf_runtime_address addr)
{
    auto alloc = p->memory.try_get(addr);
    if (alloc == nullptr)
        return false;
    alloc->is_gc_weak = false;
    return true;
}

