// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/builder.h>

#include <lauf/builtin.h>
#include <lauf/detail/bytecode.hpp>
#include <lauf/detail/bytecode_builder.hpp>
#include <lauf/detail/literal_pool.hpp>
#include <lauf/detail/stack_allocator.hpp>
#include <lauf/detail/stack_check.hpp>
#include <lauf/detail/verify.hpp>
#include <lauf/impl/module.hpp>
#include <lauf/impl/vm.hpp>
#include <vector>

using namespace lauf::_detail;

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

condition_code translate_condition(lauf_condition cond)
{
    switch (cond)
    {
    case LAUF_IS_FALSE:
        return condition_code::is_zero;
    case LAUF_IS_TRUE:
        return condition_code::is_nonzero;
    case LAUF_CMP_EQ:
        return condition_code::cmp_eq;
    case LAUF_CMP_NE:
        return condition_code::cmp_ne;
    case LAUF_CMP_LT:
        return condition_code::cmp_lt;
    case LAUF_CMP_LE:
        return condition_code::cmp_le;
    case LAUF_CMP_GT:
        return condition_code::cmp_gt;
    case LAUF_CMP_GE:
        return condition_code::cmp_ge;
    }
}
} // namespace

struct lauf_builder_impl
{
    stack_allocator     alloc;
    lauf_debug_location cur_location;

    lauf_builder_impl() : cur_location{}, bytecode(alloc), marker(alloc.top()), cur_fn(0) {}

    //=== per module ===//
    module_decl                mod;
    literal_pool               literals;
    std::vector<function_decl> functions;

    void reset_module()
    {
        mod = {};
        literals.reset();
        functions.clear();
    }

    //=== per function ===//
    stack_allocator::marker marker;
    std::size_t             cur_fn;
    stack_allocator_offset  stack_frame;
    stack_checker           value_stack;
    bytecode_builder        bytecode;

    void reset_function()
    {
        alloc.unwind(marker);
        marker = alloc.top();

        cur_fn      = 0;
        stack_frame = {};
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
    auto result            = lauf_impl_allocate_module(b->functions.size(), b->literals.size());
    result->name           = b->mod.name;
    result->path           = b->mod.path;
    result->function_count = b->functions.size();

    for (auto idx = std::size_t(0); idx != b->functions.size(); ++idx)
    {
        result->function_begin()[idx]      = b->functions[idx].fn;
        result->function_begin()[idx]->mod = result;
    }

    if (b->literals.size() != 0)
    {
        std::memcpy(result->literal_data(), b->literals.data(),
                    b->literals.size() * sizeof(lauf_value));
    }

    return result;
}

lauf_function_decl lauf_declare_function(lauf_builder b, const char* name, lauf_signature signature)
{
    auto idx = b->functions.size();
    b->functions.push_back({name, signature, nullptr});
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
    auto result              = lauf_impl_allocate_function(b->bytecode.size());
    result->name             = fn_decl.name;
    result->local_stack_size = static_cast<uint16_t>(local_size);
    result->max_vstack_size  = b->value_stack.max_stack_size();
    result->input_count      = fn_decl.signature.input_count;
    result->output_count     = fn_decl.signature.output_count;
    result->debug_locations  = b->bytecode.debug_locations();

    LAUF_VERIFY(frame_size_for(result) < stack_allocator::max_allocation_size(), "function",
                "local variables of functions exceed stack frame size limit");

    // Copy and patch bytecode.
    b->bytecode.finish(result->bytecode());

    fn_decl.fn = result;
    return result;
}

lauf_local_variable lauf_build_local_variable(lauf_builder b, lauf_layout layout)
{
    auto addr = b->stack_frame.allocate(layout.size, layout.alignment);
    return {addr};
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

    if (b->bytecode.get_cur_idom().tag.op == bc_op::call)
        b->bytecode.replace_last_instruction(bc_op::tail_call);
    else
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

void lauf_build_int(lauf_builder b, lauf_value_sint value)
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

    b->value_stack.push("int");
}

void lauf_build_ptr(lauf_builder b, lauf_value_ptr ptr)
{
    b->bytecode.location(b->cur_location);

    auto idx = b->literals.insert(ptr);
    b->bytecode.instruction(LAUF_VM_INSTRUCTION(push, idx));

    b->value_stack.push("ptr");
}

void lauf_build_local_addr(lauf_builder b, lauf_local_variable var)
{
    b->bytecode.location(b->cur_location);

    b->bytecode.instruction(LAUF_VM_INSTRUCTION(local_addr, var._addr));
    b->value_stack.push("local_addr");
}

void lauf_build_drop(lauf_builder b, size_t n)
{
    b->bytecode.location(b->cur_location);

    b->bytecode.instruction(LAUF_VM_INSTRUCTION(drop, n));
    b->value_stack.pop("drop", n);
}

void lauf_build_pick(lauf_builder b, size_t n)
{
    b->bytecode.location(b->cur_location);

    if (n == 0)
        b->bytecode.instruction(LAUF_VM_INSTRUCTION(dup));
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
        b->bytecode.instruction(LAUF_VM_INSTRUCTION(swap));
    else
        b->bytecode.instruction(LAUF_VM_INSTRUCTION(roll, n));
    LAUF_VERIFY(n < b->value_stack.cur_stack_size(), "roll", "invalid stack index");
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

    b->bytecode.instruction(LAUF_VM_INSTRUCTION(call, bc_function_idx(fn._idx)));

    auto signature = b->functions[fn._idx].signature;
    b->value_stack.pop("call", signature.input_count);
    b->value_stack.push("call", signature.output_count);
}

void lauf_build_call_builtin(lauf_builder b, struct lauf_builtin fn)
{
    b->bytecode.location(b->cur_location);

    auto idx          = b->literals.insert(reinterpret_cast<void*>(fn.impl));
    auto stack_change = int32_t(fn.signature.input_count) - int32_t(fn.signature.output_count);
    b->bytecode.instruction(LAUF_VM_INSTRUCTION(call_builtin, stack_change, idx));

    b->value_stack.pop("call_builtin", fn.signature.input_count);
    b->value_stack.push("call_builtin", fn.signature.output_count);
}

void lauf_build_array_element(lauf_builder b, lauf_type type)
{
    b->bytecode.location(b->cur_location);

    b->bytecode.instruction(LAUF_VM_INSTRUCTION(array_element, type->layout.size));

    b->value_stack.pop("array_element", 2);
    b->value_stack.push("array_element");
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

void lauf_build_load_value(lauf_builder b, lauf_local_variable var)
{
    b->bytecode.location(b->cur_location);

    // If the last instruction is a store of the same address, turn it into a save instead.
    // TODO: invalid on jump
    if (b->bytecode.get_cur_idom() == LAUF_VM_INSTRUCTION(store_value, var._addr))
        b->bytecode.replace_last_instruction(bc_op::save_value);
    else
        b->bytecode.instruction(LAUF_VM_INSTRUCTION(load_value, var._addr));
    b->value_stack.push("load_value");
}

void lauf_build_store_value(lauf_builder b, lauf_local_variable var)
{
    b->bytecode.location(b->cur_location);

    b->bytecode.instruction(LAUF_VM_INSTRUCTION(store_value, var._addr));
    b->value_stack.pop("store_value");
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

