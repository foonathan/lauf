// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/builder.h>

#include <lauf/bc/bytecode.hpp>
#include <lauf/bc/bytecode_builder.hpp>
#include <lauf/bc/literal_pool.hpp>
#include <lauf/bc/stack_check.hpp>
#include <lauf/builtin.h>
#include <lauf/impl/module.hpp>
#include <lauf/impl/vm.hpp>
#include <lauf/support/stack_allocator.hpp>
#include <lauf/support/verify.hpp>
#include <vector>

namespace
{
struct module_decl
{
    const char* name;
    const char* path;
};

struct function_decl
{
    const char*    name;
    lauf_signature signature;

    // Set to actual function once finished.
    lauf_function fn;
};

lauf::condition_code translate_condition(lauf_condition cond)
{
    switch (cond)
    {
    case LAUF_IS_FALSE:
        return lauf::condition_code::is_zero;
    case LAUF_IS_TRUE:
        return lauf::condition_code::is_nonzero;
    case LAUF_CMP_EQ:
        return lauf::condition_code::cmp_eq;
    case LAUF_CMP_NE:
        return lauf::condition_code::cmp_ne;
    case LAUF_CMP_LT:
        return lauf::condition_code::cmp_lt;
    case LAUF_CMP_LE:
        return lauf::condition_code::cmp_le;
    case LAUF_CMP_GT:
        return lauf::condition_code::cmp_gt;
    case LAUF_CMP_GE:
        return lauf::condition_code::cmp_ge;
    }
}
} // namespace

struct lauf_builder_impl
{
    lauf::memory_stack    stack;
    lauf_debug_location   cur_location;
    lauf::stack_allocator alloc;

    lauf_builder_impl()
    : cur_location{}, alloc(stack), local_allocations(alloc, 32), bytecode(alloc),
      marker(alloc.top()), cur_fn(0)
    {}

    //=== per module ===//
    module_decl                      mod;
    lauf::literal_pool               literals;
    std::vector<function_decl>       functions;
    std::vector<lauf::vm_allocation> allocations;

    void reset_module()
    {
        mod = {};
        literals.reset();
        functions.clear();
        allocations.clear();
    }

    //=== per function ===//
    lauf::stack_allocator::marker              marker;
    std::size_t                                cur_fn;
    lauf::stack_allocator_offset               stack_frame;
    lauf::temporary_array<lauf::vm_allocation> local_allocations;
    lauf::stack_checker                        value_stack;
    lauf::bytecode_builder                     bytecode;

    void reset_function()
    {
        alloc.unwind(marker);
        marker = alloc.top();

        cur_fn      = 0;
        stack_frame = {};
        local_allocations.clear_and_reserve(alloc, 32);
        value_stack = {};
        bytecode.reset();
    }
};

lauf_builder lauf_builder_create(void)
{
    return new lauf_builder_impl();
}

void lauf_builder_destroy(lauf_builder b)
{
    delete b;
}

void lauf_build_debug_location(lauf_builder b, lauf_debug_location location)
{
    b->cur_location = location;
}

void lauf_build_module(lauf_builder b, const char* name, const char* path)
{
    b->reset_module();
    b->mod = module_decl{name, path};
}

lauf_module lauf_finish_module(lauf_builder b)
{
    auto result = lauf_module_impl::create(
        {b->functions.size(), b->literals.size(), b->allocations.size()});
    result->name             = b->mod.name;
    result->path             = b->mod.path;
    result->function_count   = b->functions.size();
    result->literal_count    = b->literals.size();
    result->allocation_count = b->allocations.size();

    for (auto idx = std::size_t(0); idx != b->functions.size(); ++idx)
    {
        result->function_begin()[idx]      = b->functions[idx].fn;
        result->function_begin()[idx]->mod = result;
    }

    if (!b->literals.empty())
        std::memcpy(result->literal_data(), b->literals.data(),
                    b->literals.size() * sizeof(lauf_value));

    if (!b->allocations.empty())
        std::memcpy(result->allocation_data(), b->allocations.data(),
                    b->allocations.size() * sizeof(lauf::vm_allocation));

    return result;
}

lauf_function_decl lauf_declare_function(lauf_builder b, const char* name, lauf_signature signature)
{
    auto idx = b->functions.size();
    b->functions.push_back({name, signature, nullptr});
    return {idx};
}

lauf_global lauf_build_const(lauf_builder b, const void* memory, size_t size)
{
    LAUF_VERIFY(size < UINT32_MAX, "const", "allocation size limit exceeded");

    auto idx = b->allocations.size();
    b->allocations.emplace_back(memory, uint32_t(size));
    return {idx};
}

lauf_global lauf_build_data(lauf_builder b, const void* memory, size_t size)
{
    LAUF_VERIFY(size < UINT32_MAX, "data", "allocation size limit exceeded");

    auto idx = b->allocations.size();
    b->allocations.emplace_back(memory, uint32_t(size), lauf::vm_allocation::static_mutable_memory);
    return {idx};
}

lauf_global lauf_build_zero_data(lauf_builder b, size_t size)
{
    LAUF_VERIFY(size < UINT32_MAX, "data", "allocation size limit exceeded");

    auto idx = b->allocations.size();
    b->allocations.emplace_back(uint32_t(size));
    return {idx};
}

void lauf_build_function(lauf_builder b, lauf_function_decl fn)
{
    b->reset_function();
    b->cur_fn = fn._idx;

    b->value_stack.push("function", b->functions[b->cur_fn].signature.input_count);
}

lauf_function lauf_finish_function(lauf_builder b)
{
    auto& fn_decl = b->functions[b->cur_fn];

    // We need to align the end of the stack frame properly for the next one.
    b->stack_frame.align_to(alignof(void*));
    auto local_size = b->stack_frame.size();

    // Allocate and set function members.
    auto result               = lauf_function_impl::create({b->bytecode.size(), 0});
    result->name              = fn_decl.name;
    result->local_stack_size  = static_cast<uint16_t>(local_size);
    result->max_vstack_size   = b->value_stack.max_stack_size();
    result->instruction_count = b->bytecode.size();
    result->input_count       = fn_decl.signature.input_count;
    result->output_count      = fn_decl.signature.output_count;
    result->debug_locations   = b->bytecode.debug_locations();

    LAUF_VERIFY(lauf::frame_size_for(result) < lauf::stack_allocator::max_allocation_size(),
                "function", "local variables of functions exceed stack frame size limit");
    std::memcpy(result->local_allocations(), b->local_allocations.data(),
                b->local_allocations.size() * sizeof(lauf::vm_allocation));
    result->local_allocation_count = b->local_allocations.size();

    // Copy and patch bytecode.
    if (result->local_allocation_count > 0)
        b->bytecode.replace_entry_instruction(LAUF_VM_INSTRUCTION(add_local_allocations));
    b->bytecode.finish(result->bytecode(), result->local_allocation_count > 0);

    fn_decl.fn = result;
    return result;
}

lauf_local lauf_build_local_variable(lauf_builder b, lauf_layout layout)
{
    auto addr = b->stack_frame.allocate(layout.size, layout.alignment);
    return {addr, layout.size};
}

lauf_label lauf_declare_label(lauf_builder b, size_t vstack_size)
{
    return b->bytecode.declare_label(vstack_size);
}

void lauf_place_label(lauf_builder b, lauf_label label)
{
    b->bytecode.place_label(label);

    // If the label can be reached by fallthrough, we need to check that the current stack size
    // matches.
    auto check_stack_size = [&] {
        if (b->bytecode.can_fallthrough())
            // Last instruction can fallthrough the label; stack size needs to match.
            return b->value_stack.cur_stack_size() == b->bytecode.get_label_stack_size(label);
        else
            // Last instruction is an unconditional jump; can't be reached by fallthrough.
            return true;
    };
    LAUF_VERIFY(check_stack_size(), "label", "expected value stack size not matched");
    b->value_stack.set_stack_size("label", b->bytecode.get_label_stack_size(label));
}

void lauf_build_return(lauf_builder b)
{
    b->bytecode.location(b->cur_location);

    b->bytecode.instruction(LAUF_VM_INSTRUCTION(return_));

    auto output_count = b->functions[b->cur_fn].signature.output_count;
    LAUF_VERIFY(b->value_stack.cur_stack_size() == output_count, "return",
                "value stack size does not match signature");
    b->value_stack.pop("return", output_count);
}

void lauf_build_jump(lauf_builder b, lauf_label dest)
{
    b->bytecode.location(b->cur_location);

    b->bytecode.instruction(LAUF_VM_INSTRUCTION(jump, uint32_t(dest._idx)));

    LAUF_VERIFY(b->value_stack.cur_stack_size() == b->bytecode.get_label_stack_size(dest), "jump",
                "value stack size does not match label");
}

void lauf_build_jump_if(lauf_builder b, lauf_condition condition, lauf_label dest)
{
    b->bytecode.location(b->cur_location);

    auto cc = translate_condition(condition);
    b->bytecode.instruction(LAUF_VM_INSTRUCTION(jump_if, cc, uint32_t(dest._idx)));

    b->value_stack.pop("jump_if");
    LAUF_VERIFY(b->value_stack.cur_stack_size() == b->bytecode.get_label_stack_size(dest),
                "jump_if", "value stack size does not match label");
}

void lauf_build_sint(lauf_builder b, lauf_value_sint value)
{
    b->bytecode.location(b->cur_location);

    if (value == 0)
    {
        b->bytecode.instruction(LAUF_VM_INSTRUCTION(push_zero));
    }
    else if (0 < value && value <= 0xFF'FFFF)
    {
        b->bytecode.instruction(LAUF_VM_INSTRUCTION(push_small_zext, uint32_t(value)));
    }
    else if (-0xFF'FFFF <= value && value < 0)
    {
        b->bytecode.instruction(LAUF_VM_INSTRUCTION(push_small_neg, uint32_t(-value)));
    }
    else
    {
        auto idx = b->literals.insert(value);
        b->bytecode.instruction(LAUF_VM_INSTRUCTION(push, idx));
    }

    b->value_stack.push("sint");
}

void lauf_build_uint(lauf_builder b, lauf_value_uint value)
{
    b->bytecode.location(b->cur_location);

    if (value == 0)
    {
        b->bytecode.instruction(LAUF_VM_INSTRUCTION(push_zero));
    }
    else if (0 < value && value <= 0xFF'FFFF)
    {
        b->bytecode.instruction(LAUF_VM_INSTRUCTION(push_small_zext, uint32_t(value)));
    }
    else
    {
        auto idx = b->literals.insert(value);
        b->bytecode.instruction(LAUF_VM_INSTRUCTION(push, idx));
    }

    b->value_stack.push("uint");
}

void lauf_build_global_addr(lauf_builder b, lauf_global global)
{
    b->bytecode.location(b->cur_location);

    if (global._idx > 0xFF'FFFF)
    {
        auto lit_idx = b->literals.insert(lauf_value_address{uint32_t(global._idx), 0});
        b->bytecode.instruction(LAUF_VM_INSTRUCTION(push, lit_idx));
    }
    else
    {
        b->bytecode.instruction(LAUF_VM_INSTRUCTION(push_addr, global._idx));
    }

    b->value_stack.push("global_addr");
}

void lauf_build_local_addr(lauf_builder b, lauf_local var)
{
    b->bytecode.location(b->cur_location);

    auto idx = [&] {
        auto addr   = reinterpret_cast<void*>(var._addr); // NOLINT
        auto result = 0;
        for (auto& alloc : b->local_allocations)
        {
            if (alloc.ptr == addr)
                return result;
            ++result;
        }

        lauf::vm_allocation local(addr, var._size, lauf::vm_allocation::stack_memory);
        b->local_allocations.push_back(b->alloc, local);
        return result;
    }();

    b->bytecode.instruction(LAUF_VM_INSTRUCTION(push_local_addr, idx));
    b->value_stack.push("local_addr");
}

void lauf_build_layout_of(lauf_builder b, lauf_type type)
{
    b->bytecode.location(b->cur_location);

    b->bytecode.instruction(LAUF_VM_INSTRUCTION(push_small_zext, type->layout.size));
    b->bytecode.instruction(LAUF_VM_INSTRUCTION(push_small_zext, type->layout.alignment));

    b->value_stack.push("layout_of", 2);
}

void lauf_build_pop(lauf_builder b, size_t n)
{
    b->bytecode.location(b->cur_location);

    b->bytecode.instruction(LAUF_VM_INSTRUCTION(pop, n));
    b->value_stack.pop("pop", n);
}

void lauf_build_pick(lauf_builder b, size_t n)
{
    b->bytecode.location(b->cur_location);

    if (n == 0)
        b->bytecode.instruction(LAUF_VM_INSTRUCTION(dup, n));
    else
        b->bytecode.instruction(LAUF_VM_INSTRUCTION(pick, n));
    LAUF_VERIFY(n < b->value_stack.cur_stack_size(), "pick", "invalid stack index");
    b->value_stack.push("pick");
}

void lauf_build_roll(lauf_builder b, size_t n)
{
    b->bytecode.location(b->cur_location);

    if (n == 0)
    {} // noop
    else if (n == 1)
        b->bytecode.instruction(LAUF_VM_INSTRUCTION(swap, n));
    else
        b->bytecode.instruction(LAUF_VM_INSTRUCTION(roll, n));
    LAUF_VERIFY(n < b->value_stack.cur_stack_size(), "roll", "invalid stack index");
}

void lauf_build_drop(lauf_builder b, size_t n)
{
    lauf_build_roll(b, n);
    lauf_build_pop(b, 1);
}

void lauf_build_select(lauf_builder b, size_t max_index)
{
    b->bytecode.location(b->cur_location);

    LAUF_VERIFY(max_index >= 2, "select", "invalid max index for select");
    if (max_index == 2)
        b->bytecode.instruction(LAUF_VM_INSTRUCTION(select2));
    else
        b->bytecode.instruction(LAUF_VM_INSTRUCTION(select, max_index));

    b->value_stack.pop("select", max_index + 1);
    b->value_stack.push("select");
}

void lauf_build_select_if(lauf_builder b, lauf_condition condition)
{
    b->bytecode.location(b->cur_location);

    if (condition == LAUF_IS_TRUE)
        b->bytecode.instruction(LAUF_VM_INSTRUCTION(select2));
    else
        b->bytecode.instruction(LAUF_VM_INSTRUCTION(select_if, translate_condition(condition)));

    b->value_stack.pop("select", 2 + 1);
    b->value_stack.push("select");
}

void lauf_build_call(lauf_builder b, lauf_function_decl fn)
{
    b->bytecode.location(b->cur_location);

    b->bytecode.instruction(LAUF_VM_INSTRUCTION(call, lauf::bc_function_idx(fn._idx)));

    auto signature = b->functions[fn._idx].signature;
    b->value_stack.pop("call", signature.input_count);
    b->value_stack.push("call", signature.output_count);
}

void lauf_build_call_builtin(lauf_builder b, struct lauf_builtin fn)
{
    b->bytecode.location(b->cur_location);

    auto diff = reinterpret_cast<unsigned char*>(fn.impl)
                - reinterpret_cast<unsigned char*>(&lauf_builtin_finish);
    auto addr = diff / 16;
    if (diff % 16 != 0 || addr < INT16_MIN || addr > INT16_MAX)
    {
        auto idx = b->literals.insert(reinterpret_cast<void*>(fn.impl));
        b->bytecode.instruction(LAUF_VM_INSTRUCTION(call_builtin_long, fn.signature, idx));
    }
    else
    {
        b->bytecode.instruction(LAUF_VM_INSTRUCTION(call_builtin, fn.signature, int32_t(addr)));
    }

    b->value_stack.pop("call_builtin", fn.signature.input_count);
    b->value_stack.push("call_builtin", fn.signature.output_count);
}

void lauf_build_array_element_addr(lauf_builder b, lauf_layout layout)
{
    b->bytecode.location(b->cur_location);

    b->bytecode.instruction(
        LAUF_VM_INSTRUCTION(array_element_addr,
                            lauf::round_to_multiple_of_alignment(layout.size, layout.alignment)));

    b->value_stack.pop("array_element_addr", 2);
    b->value_stack.push("array_element_addr");
}

void lauf_build_aggregate_member_addr(lauf_builder b, size_t member_offset)
{
    b->bytecode.location(b->cur_location);

    b->bytecode.instruction(LAUF_VM_INSTRUCTION(aggregate_member_addr, member_offset));

    b->value_stack.pop("aggregate_member_addr");
    b->value_stack.push("aggregate_member_addr");
}

void lauf_build_load_field(lauf_builder b, lauf_type type, size_t field)
{
    b->bytecode.location(b->cur_location);
    LAUF_VERIFY(field < type->field_count, "store_field", "invalid field count for type");

    auto idx = b->literals.insert(type);
    b->bytecode.instruction(LAUF_VM_INSTRUCTION(load_field, field, idx));

    b->value_stack.pop("load_field");
    b->value_stack.push("load_field");
}

void lauf_build_store_field(lauf_builder b, lauf_type type, size_t field)
{
    b->bytecode.location(b->cur_location);
    LAUF_VERIFY(field < type->field_count, "store_field", "invalid field count for type");

    auto idx = b->literals.insert(type);
    b->bytecode.instruction(LAUF_VM_INSTRUCTION(store_field, field, idx));

    b->value_stack.pop("store_field", 2);
}

namespace
{
void build_load_value_impl(const char* name, lauf_builder b, lauf_local var, size_t member_offset)
{
    LAUF_VERIFY(member_offset + sizeof(lauf_value) <= var._size
                    && (var._addr + member_offset) % alignof(lauf_value) == 0,
                name, "wrong layout");
    b->bytecode.location(b->cur_location);

    // If the last instruction is a store of the same address, turn it into a save instead.
    if (b->bytecode.get_cur_idom() == LAUF_VM_INSTRUCTION(store_value, var._addr + member_offset))
        b->bytecode.replace_last_instruction(lauf::bc_op::save_value);
    else
        b->bytecode.instruction(LAUF_VM_INSTRUCTION(load_value, var._addr + member_offset));
    b->value_stack.push(name);
}
} // namespace

void lauf_build_load_value(lauf_builder b, lauf_local var)
{
    build_load_value_impl("load_value", b, var, 0);
}

void lauf_build_load_aggregate_value(lauf_builder b, lauf_local var, size_t member_offset)
{
    build_load_value_impl("load_aggregate_value", b, var, member_offset);
}

void lauf_build_load_array_value(lauf_builder b, lauf_local var)
{
    LAUF_VERIFY(var._size >= sizeof(lauf_value), "load_array_value", "wrong layout");
    b->bytecode.location(b->cur_location);

    b->bytecode.instruction(LAUF_VM_INSTRUCTION(load_array_value, var._addr));

    b->value_stack.pop("load_array_value");
    b->value_stack.push("load_array_value");
}

namespace
{
void build_store_value_impl(const char* name, lauf_builder b, lauf_local var, size_t member_offset)
{
    LAUF_VERIFY(member_offset + sizeof(lauf_value) <= var._size
                    && (var._addr + member_offset) % alignof(lauf_value) == 0,
                name, "wrong layout");
    b->bytecode.location(b->cur_location);

    b->bytecode.instruction(LAUF_VM_INSTRUCTION(store_value, var._addr + member_offset));
    b->value_stack.pop(name);
}
} // namespace

void lauf_build_store_value(lauf_builder b, lauf_local var)
{
    build_store_value_impl("store_value", b, var, 0);
}

void lauf_build_store_aggregate_value(lauf_builder b, lauf_local var, size_t member_offset)
{
    build_store_value_impl("store_aggregate_value", b, var, member_offset);
}

void lauf_build_store_array_value(lauf_builder b, lauf_local var)
{
    LAUF_VERIFY(var._size >= sizeof(lauf_value), "store_array_value", "wrong layout");
    b->bytecode.location(b->cur_location);

    b->bytecode.instruction(LAUF_VM_INSTRUCTION(store_array_value, var._addr));
    b->value_stack.pop("store_array_value", 2);
}

void lauf_build_panic(lauf_builder b)
{
    b->bytecode.location(b->cur_location);

    b->bytecode.instruction(LAUF_VM_INSTRUCTION(panic));
    b->value_stack.pop("panic");
}

void lauf_build_panic_if(lauf_builder b, lauf_condition condition)
{
    b->bytecode.location(b->cur_location);

    b->bytecode.instruction(LAUF_VM_INSTRUCTION(panic_if, translate_condition(condition)));
    b->value_stack.pop("panic_if", 2);
}

