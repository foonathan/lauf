// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/vm.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <lauf/builtin.h>
#include <lauf/detail/bytecode.hpp>
#include <lauf/detail/stack_allocator.hpp>
#include <lauf/impl/module.hpp>
#include <lauf/impl/process.hpp>
#include <lauf/impl/program.hpp>
#include <lauf/impl/vm.hpp>
#include <lauf/type.h>
#include <type_traits>
#include <vector>

using namespace lauf::_detail;

namespace
{
class vm_memory
{
public:
    vm_memory()
    {
        _allocs.reserve(1024);
    }

    void reset(lauf_program prog)
    {
        _allocs.clear();

        auto                   static_memory = prog->static_memory();
        stack_allocator_offset allocator;
        for (auto ptr = prog->mod->allocation_data();
             ptr != prog->mod->allocation_data() + prog->mod->allocation_count; ++ptr)
        {
            auto alloc = *ptr;
            if ((alloc.flags & allocation::static_memory) != 0)
            {
                auto offset = allocator.allocate(alloc.size);
                if ((alloc.flags & allocation::clear_memory) != 0)
                    std::memset(static_memory + offset, 0, alloc.size);
                else if ((alloc.flags & allocation::copy_memory) != 0)
                    std::memcpy(static_memory + offset, alloc.ptr, alloc.size);
                alloc.ptr = static_memory + offset;
            }

            _allocs.push_back(alloc);
        }
    }

    void local_allocation(void* memory, uint32_t size)
    {
        _allocs.emplace_back(memory, size);
    }
    void return_()
    {
        _allocs.pop_back();
    }

    lauf_value_address local_address(size_t offset) const
    {
        auto allocation = _allocs.size() - 1;
        return {uint32_t(allocation), uint32_t(offset)};
    }

    bool poison(lauf_value_address addr)
    {
        if (auto alloc = get_allocation(addr))
        {
            alloc->flags |= allocation::is_poisoned;
            return true;
        }

        return false;
    }
    bool unpoison(lauf_value_address addr)
    {
        if (auto alloc = get_allocation(addr))
        {
            alloc->flags &= ~allocation::is_poisoned;
            return true;
        }

        return false;
    }

    const void* get_const_ptr(lauf_value_address addr, size_t size) const
    {
        if (auto alloc = get_allocation(addr))
        {
            if (ptrdiff_t(alloc->size) - ptrdiff_t(addr.offset) < size
                || (alloc->flags & allocation::is_poisoned) != 0)
                return nullptr;

            return alloc->offset(addr.offset);
        }

        return nullptr;
    }
    void* get_mutable_ptr(lauf_value_address addr, size_t size) const
    {
        if (auto alloc = get_allocation(addr))
        {
            if (ptrdiff_t(alloc->size) - ptrdiff_t(addr.offset) < size
                || (alloc->flags & allocation::is_poisoned) != 0
                || (alloc->flags & allocation::is_const) != 0)
                return nullptr;

            return alloc->offset(addr.offset);
        }

        return nullptr;
    }

    const char* get_const_cstr(lauf_value_address addr) const
    {
        if (auto alloc = get_allocation(addr))
        {
            if (addr.offset >= alloc->size)
                return nullptr;

            auto cstr = static_cast<const char*>(alloc->offset(addr.offset));
            if (std::memchr(cstr, 0, alloc->size - addr.offset) == nullptr)
                return nullptr;
            return cstr;
        }

        return nullptr;
    }
    const char* get_mutable_cstr(lauf_value_address addr) const
    {
        if (auto alloc = get_allocation(addr))
        {
            if (addr.offset >= alloc->size)
                return nullptr;

            auto cstr = static_cast<const char*>(alloc->offset(addr.offset));
            if (std::memchr(cstr, 0, alloc->size - addr.offset) == nullptr)
                return nullptr;
            return cstr;
        }

        return nullptr;
    }

private:
    const allocation* get_allocation(lauf_value_address addr) const
    {
        if (addr.allocation >= _allocs.size())
            return nullptr;
        else
            return &_allocs[addr.allocation];
    }
    allocation* get_allocation(lauf_value_address addr)
    {
        if (addr.allocation >= _allocs.size())
            return nullptr;
        else
            return &_allocs[addr.allocation];
    }

    std::vector<allocation> _allocs;
};
} // namespace

struct alignas(lauf_value) lauf_vm_impl
{
    lauf_vm_process    process;
    lauf_panic_handler panic_handler;
    size_t             value_stack_size;
    stack_allocator    memory_stack;

    lauf_vm_impl(lauf_vm_options options)
    : panic_handler(options.panic_handler),
      value_stack_size(options.max_value_stack_size / sizeof(lauf_value)),
      memory_stack(options.max_stack_size)
    {
        process = create_null_process(this);
    }

    lauf_vm_impl(const lauf_vm_impl&) = delete;
    lauf_vm_impl& operator=(const lauf_vm_impl&) = delete;

    ~lauf_vm_impl()
    {
        destroy_process(process);
    }

    lauf_value* value_stack()
    {
        return reinterpret_cast<lauf_value*>(this + 1) + value_stack_size;
    }
    lauf_value* value_stack_limit()
    {
        return reinterpret_cast<lauf_value*>(this + 1);
    }
};

namespace
{
struct alignas(void*) stack_frame
{
    lauf_function           fn;
    lauf_vm_instruction*    return_ip;
    stack_allocator::marker unwind;
    stack_frame*            prev;
};

void* new_stack_frame(lauf_vm_process& process, void* frame_ptr, lauf_vm_instruction* return_ip,
                      lauf_function fn)
{
    auto vm         = process->vm;
    auto marker     = vm->memory_stack.top();
    auto prev_frame = static_cast<stack_frame*>(frame_ptr) - 1;

    // As the local_stack_size is a multiple of max alignment, we don't need to worry about aligning
    // it; the builder takes care of it when computing the stack size.
    auto memory = vm->memory_stack.allocate(lauf::_detail::frame_size_for(fn));
    if (memory == nullptr)
        return nullptr;

    auto frame = ::new (memory) stack_frame{fn, return_ip, marker, prev_frame};
    add_allocation(process, {frame + 1, fn->local_stack_size});
    return frame + 1;
}
} // namespace

std::size_t lauf::_detail::frame_size_for(lauf_function fn)
{
    return sizeof(stack_frame) + fn->local_stack_size;
}

void lauf::_detail::update_process(lauf_vm vm, lauf_vm_process process)
{
    vm->process = process;
}

//=== backtrace ===//
// lauf_backtrace is a pointer to a stack frame.
// It is initially a pointer to a dummy stack frame whose return_ip stores the initial ip + 1,
// and prev->fn is the current function.

lauf_function lauf_backtrace_get_function(lauf_backtrace bt)
{
    auto frame = static_cast<stack_frame*>(bt);
    return frame->prev->fn;
}

lauf_debug_location lauf_backtrace_get_location(lauf_backtrace bt)
{
    auto frame = static_cast<stack_frame*>(bt);
    return lauf_function_get_location_of(frame->prev->fn, frame->return_ip - 1);
}

lauf_backtrace lauf_backtrace_parent(lauf_backtrace bt)
{
    auto frame = static_cast<stack_frame*>(bt);
    // We need to go to the parent.
    auto next_bt = frame->prev;

    if (next_bt->return_ip == nullptr)
        return nullptr;
    else
        return next_bt;
}

//=== panic handler ===//
struct lauf_panic_info_impl
{
    stack_frame fake_frame;
};

namespace
{
lauf_panic_info_impl make_panic_info(void* frame_ptr, lauf_vm_instruction* ip)
{
    auto cur_frame = static_cast<stack_frame*>(frame_ptr) - 1;
    return {{nullptr, ip + 1, {}, cur_frame}};
}
} // namespace

lauf_backtrace lauf_panic_info_get_backtrace(lauf_panic_info info)
{
    return &info->fake_frame;
}

//=== vm functions ===//
const lauf_vm_options lauf_default_vm_options
    = {size_t(128) * 1024, size_t(896) * 1024, [](lauf_panic_info info, const char* message) {
           std::fprintf(stderr, "[lauf] panic: %s\n",
                        message == nullptr ? "(invalid message address)" : message);
           std::fprintf(stderr, "stack backtrace\n");

           auto index = 0;
           for (auto cur = lauf_panic_info_get_backtrace(info); cur != nullptr;
                cur      = lauf_backtrace_parent(cur))
           {
               auto fn = lauf_backtrace_get_function(cur);
               std::fprintf(stderr, " %4d. %s\n", index, lauf_function_get_name(fn));
               if (auto loc = lauf_backtrace_get_location(cur); loc.line != 0u && loc.column != 0u)
               {
                   auto path = lauf_module_get_path(lauf_function_get_module(fn));
                   std::fprintf(stderr, "        at %s:%u:%u\n", path, unsigned(loc.line),
                                unsigned(loc.column));
               }
               ++index;
           }
       }};

lauf_vm lauf_vm_create(lauf_vm_options options)
{
    auto memory = ::operator new(sizeof(lauf_vm_impl) + options.max_value_stack_size);
    return ::new (memory) lauf_vm_impl(options);
}

void lauf_vm_destroy(lauf_vm vm)
{
    vm->~lauf_vm_impl();
    ::operator delete(vm);
}

void lauf_vm_set_panic_handler(lauf_vm vm, lauf_panic_handler handler)
{
    vm->panic_handler = handler;
}

//=== execute ===//
namespace
{
bool check_condition(condition_code cc, lauf_value value)
{
    switch (cc)
    {
    case condition_code::is_zero:
        return value.as_sint == 0;
    case condition_code::is_nonzero:
        return value.as_sint != 0;
    case condition_code::cmp_lt:
        return value.as_sint < 0;
    case condition_code::cmp_le:
        return value.as_sint <= 0;
    case condition_code::cmp_gt:
        return value.as_sint > 0;
    case condition_code::cmp_ge:
        return value.as_sint >= 0;
    }
}
} // namespace

#if LAUF_HAS_TAIL_CALL
// Each instruction is a function that tail calls the next one.

namespace
{
bool dispatch(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr,
              lauf_vm_process process);

#    define LAUF_BC_OP(Name, Data, ...)                                                            \
        bool execute_##Name(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr,      \
                            lauf_vm_process process) __VA_ARGS__
#    define LAUF_DISPATCH LAUF_TAIL_CALL return dispatch(ip, vstack_ptr, frame_ptr, process)
#    define LAUF_DISPATCH_BUILTIN(Callee, StackChange)                                             \
        LAUF_TAIL_CALL return Callee(ip, vstack_ptr, frame_ptr, process)

#    include "lauf/detail/bc_ops.h"

#    undef LAUF_BC_OP
#    undef LAUF_DISPATCH
#    undef LAUF_DISPATCH_BUILTIN

constexpr lauf_builtin_function* inst_fns[] = {
#    define LAUF_BC_OP(Name, Data, ...) &execute_##Name,
#    include "lauf/detail/bc_ops.h"
#    undef LAUF_BC_OP
};

bool dispatch(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr,
              lauf_vm_process process)
{
    if (ip == nullptr)
        return true;
    LAUF_TAIL_CALL return inst_fns[size_t(ip->tag.op)](ip, vstack_ptr, frame_ptr, process);
}
} // namespace

bool lauf_builtin_dispatch(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr,
                           lauf_vm_process process)
{
    // ip can't be null after a builtin as it can't return.
    LAUF_TAIL_CALL return inst_fns[size_t(ip->tag.op)](ip, vstack_ptr, frame_ptr, process);
}

#elif LAUF_HAS_COMPUTED_GOTO
// Use computed goto for dispatch.

namespace
{
bool dispatch(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr,
              lauf_vm_process process)
{
    void* labels[] = {
#    define LAUF_BC_OP(Name, Data, ...) &&execute_##Name,
#    include "lauf/detail/bc_ops.h"
#    undef LAUF_BC_OP
    };

    goto* labels[size_t(ip->tag.op)];

#    define LAUF_BC_OP(Name, Data, ...) execute_##Name : __VA_ARGS__
#    define LAUF_DISPATCH                                                                          \
        do                                                                                         \
        {                                                                                          \
            if (ip == nullptr)                                                                     \
                return true;                                                                       \
            goto* labels[size_t(ip->tag.op)];                                                      \
        } while (0)
#    define LAUF_DISPATCH_BUILTIN(Callee, StackChange)                                             \
        do                                                                                         \
        {                                                                                          \
            Callee(ip, vstack_ptr, frame_ptr, process);                                            \
            vstack_ptr += (StackChange);                                                           \
            goto* labels[size_t(ip->tag.op)];                                                      \
        } while (0);

#    include "lauf/detail/bc_ops.h"

#    undef LAUF_BC_OP
#    undef LAUF_DISPATCH
#    undef LAUF_DISPATCH_BUILTIN
}
} // namespace

bool lauf_builtin_dispatch(lauf_vm_instruction*, lauf_value*, void*, lauf_vm_process)
{
    // We terminate the call here.
    return true;
}

#else
// Simple switch statement in a loop.

namespace
{
bool dispatch(lauf_vm_instruction* ip, lauf_value* vstack_ptr, void* frame_ptr,
              lauf_vm_process process)
{
    while (ip != nullptr)
    {
        switch (ip->tag.op)
        {
#    define LAUF_BC_OP(Name, Data, ...)                                                            \
    case bc_op::Name:                                                                              \
        __VA_ARGS__
#    define LAUF_DISPATCH break
#    define LAUF_DISPATCH_BUILTIN(Callee, StackChange)                                             \
        {                                                                                          \
            Callee(ip, vstack_ptr, frame_ptr, process);                                            \
            vstack_ptr += (StackChange);                                                           \
            break
        }

#    include "lauf/detail/bc_ops.h"

#    undef LAUF_BC_OP
#    undef LAUF_DISPATCH
#    undef LAUF_DISPATCH_FN
    }
}

return true;
} // namespace
} // namespace

bool lauf_builtin_dispatch(lauf_vm_instruction*, lauf_value*, void*, lauf_vm_process)
{
    // We terminate the call here.
    return true;
}

#endif

bool lauf_builtin_panic(lauf_vm_process process, lauf_vm_instruction* ip, void* frame_ptr,
                        const char* message)
{
    // call_builtin has already incremented ip, so undo it to get the location.
    auto info = make_panic_info(frame_ptr, ip - 1);
    process->vm->panic_handler(&info, message);
    return false;
}

bool lauf_vm_execute(lauf_vm vm, lauf_program prog, const lauf_value* input, lauf_value* output)
{
    init_process(vm->process, prog);
    auto vstack_ptr = vm->value_stack();

    for (auto i = 0; i != prog->entry->input_count; ++i)
    {
        --vstack_ptr;
        vstack_ptr[0] = input[i];
    }

    stack_frame prev{};
    auto        frame_ptr = new_stack_frame(vm->process, &prev + 1, nullptr, prog->entry);
    assert(frame_ptr); // initial stack frame should fit in first block

    auto ip = prog->entry->bytecode();
    if (!dispatch(ip, vstack_ptr, frame_ptr, vm->process))
        return false;

    vstack_ptr = vm->value_stack() - prog->entry->output_count;
    for (auto i = 0; i != prog->entry->output_count; ++i)
    {
        output[i] = vstack_ptr[0];
        ++vstack_ptr;
    }

    vm->memory_stack.reset();
    return true;
}

