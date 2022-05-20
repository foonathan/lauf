// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/builder.h>

#include <lauf/builtin.h>
#include <lauf/detail/bytecode.hpp>
#include <lauf/detail/literal_pool.hpp>
#include <lauf/detail/stack_allocator.hpp>
#include <lauf/detail/stack_check.hpp>
#include <lauf/detail/verify.hpp>
#include <lauf/impl/module.hpp>
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
    const char*         name;
    lauf_signature      signature;
    lauf_debug_location location;

    // Set to actual function once finished.
    lauf_function fn;
};

struct label_decl
{
    size_t vstack_size;

    // Set once the label is placed.
    ptrdiff_t bytecode_offset;
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
    lauf_debug_location cur_location;

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
    std::size_t                     cur_fn;
    stack_allocator_offset          stack_frame;
    stack_checker                   value_stack;
    std::vector<label_decl>         labels;
    std::vector<bc_inst>            bytecode;
    std::vector<debug_location_map> bc_locations;

    void reset_function()
    {
        cur_fn      = 0;
        stack_frame = {};
        value_stack = {};
        labels.clear();
        bytecode.clear();
        bc_locations.clear();
    }

    void update_location()
    {
        if (bc_locations.empty()
            || std::memcmp(&bc_locations.back().location, &cur_location,
                           sizeof(lauf_debug_location))
                   != 0)
            bc_locations.push_back({ptrdiff_t(bytecode.size()), cur_location});
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
    b->functions.push_back({name, signature, {}, nullptr});
    return {idx};
}

void lauf_build_function(lauf_builder b, lauf_function_decl fn)
{
    b->reset_function();
    b->cur_fn                        = fn._idx;
    b->functions[b->cur_fn].location = b->cur_location;

    b->value_stack.push("function", b->functions[b->cur_fn].signature.input_count);
}

lauf_function lauf_finish_function(lauf_builder b)
{
    auto& fn_decl = b->functions[b->cur_fn];

    // We need to align the end of the stack frame properly for the next one.
    b->stack_frame.align_to(alignof(std::max_align_t));
    auto local_size = b->stack_frame.size();

    // Allocate and set function members.
    auto result              = lauf_impl_allocate_function(b->bytecode.size());
    result->name             = fn_decl.name;
    result->local_stack_size = static_cast<uint16_t>(local_size);
    result->max_vstack_size  = b->value_stack.max_stack_size();
    result->input_count      = fn_decl.signature.input_count;
    result->output_count     = fn_decl.signature.output_count;

    // Copy and patch bytecode.
    {
        auto cur_offset = ptrdiff_t(0);
        for (auto inst : b->bytecode)
        {
            if (inst.tag.op == bc_op::jump)
            {
                auto dest = b->labels[inst.jump.offset].bytecode_offset;
                inst      = LAUF_VM_INSTRUCTION(jump, dest - cur_offset);
            }
            else if (inst.tag.op == bc_op::jump_if)
            {
                auto dest = b->labels[inst.jump_if.offset].bytecode_offset;
                inst      = LAUF_VM_INSTRUCTION(jump_if, inst.jump_if.cc, dest - cur_offset);
            }

            result->bytecode()[cur_offset] = inst;
            ++cur_offset;
        }
    }

    // Set debug locations.
    {
        auto debug_location_size = 2 + b->bc_locations.size();
        result->debug_locations  = new debug_location_map[debug_location_size];

        auto ptr = result->debug_locations;
        *ptr++   = debug_location_map{-1, fn_decl.location};
        std::memcpy(ptr, b->bc_locations.data(),
                    b->bc_locations.size() * sizeof(debug_location_map));
        ptr += b->bc_locations.size();
        *ptr++ = debug_location_map{-1, {}};
    }

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
    auto idx = b->labels.size();
    b->labels.push_back({vstack_size, 0});
    return {idx};
}

void lauf_place_label(lauf_builder b, lauf_label label)
{
    auto& decl           = b->labels[label._idx];
    decl.bytecode_offset = ptrdiff_t(b->bytecode.size());

    // If the label can be reached by fallthrough, we need to check that the current stack size
    // matches.
    auto check_stack_size = [&] {
        if (b->bytecode.empty())
            // No instruction; can't be reached by fallthrough.
            return true; // NOLINT
        else if (auto last_op = b->bytecode.back().tag.op;
                 last_op == bc_op::jump || last_op == bc_op::return_ || last_op == bc_op::panic)
            // Last instruction is an unconditional jump; can't be reached by fallthrough.
            return true;
        else
            // Last instruction can fallthrough the label; stack size needs to match.
            return b->value_stack.cur_stack_size() == decl.vstack_size;
    };
    LAUF_VERIFY(check_stack_size(), "label", "expected value stack size not matched");
    b->value_stack.set_stack_size("label", decl.vstack_size);
}

void lauf_build_return(lauf_builder b)
{
    b->update_location();

    b->bytecode.push_back(LAUF_VM_INSTRUCTION(return_));

    auto output_count = b->functions[b->cur_fn].signature.output_count;
    LAUF_VERIFY(b->value_stack.cur_stack_size() == output_count, "return",
                "value stack size does not match signature");
    b->value_stack.pop("return", output_count);
}

void lauf_build_jump(lauf_builder b, lauf_label dest)
{
    b->update_location();

    b->bytecode.push_back(LAUF_VM_INSTRUCTION(jump, uint32_t(dest._idx)));

    LAUF_VERIFY(b->value_stack.cur_stack_size() == b->labels[dest._idx].vstack_size, "jump",
                "value stack size does not match label");
}

void lauf_build_jump_if(lauf_builder b, lauf_condition condition, lauf_label dest)
{
    b->update_location();

    auto cc = translate_condition(condition);
    b->bytecode.push_back(LAUF_VM_INSTRUCTION(jump_if, cc, uint32_t(dest._idx)));

    b->value_stack.pop("jump_if");
    LAUF_VERIFY(b->value_stack.cur_stack_size() == b->labels[dest._idx].vstack_size, "jump_if",
                "value stack size does not match label");
}

void lauf_build_int(lauf_builder b, lauf_value_sint value)
{
    b->update_location();

    if (value == 0)
    {
        b->bytecode.push_back(LAUF_VM_INSTRUCTION(push_zero));
    }
    else if (0 < value && value <= 0xFF'FFFF)
    {
        b->bytecode.push_back(LAUF_VM_INSTRUCTION(push_small_zext, uint32_t(value)));
    }
    else if (-0xFF'FFFF <= value && value < 0)
    {
        b->bytecode.push_back(LAUF_VM_INSTRUCTION(push_small_neg, uint32_t(-value)));
    }
    else
    {
        auto idx = b->literals.insert(value);
        b->bytecode.push_back(LAUF_VM_INSTRUCTION(push, idx));
    }

    b->value_stack.push("int");
}

void lauf_build_ptr(lauf_builder b, lauf_value_ptr ptr)
{
    b->update_location();

    auto idx = b->literals.insert(ptr);
    b->bytecode.push_back(LAUF_VM_INSTRUCTION(push, idx));

    b->value_stack.push("ptr");
}

void lauf_build_local_addr(lauf_builder b, lauf_local_variable var)
{
    b->update_location();

    b->bytecode.push_back(LAUF_VM_INSTRUCTION(local_addr, var._addr));
    b->value_stack.push("local_addr");
}

void lauf_build_drop(lauf_builder b, size_t n)
{
    b->update_location();

    b->bytecode.push_back(LAUF_VM_INSTRUCTION(drop, n));
    b->value_stack.pop("drop", n);
}

void lauf_build_pick(lauf_builder b, size_t n)
{
    b->update_location();

    if (n == 0)
        b->bytecode.push_back(LAUF_VM_INSTRUCTION(dup));
    else
        b->bytecode.push_back(LAUF_VM_INSTRUCTION(pick, n));
    LAUF_VERIFY(n < b->value_stack.cur_stack_size(), "pick", "invalid stack index");
    b->value_stack.push("pick");
}

void lauf_build_roll(lauf_builder b, size_t n)
{
    b->update_location();

    if (n == 0)
    {} // noop
    else if (n == 1)
        b->bytecode.push_back(LAUF_VM_INSTRUCTION(swap));
    else
        b->bytecode.push_back(LAUF_VM_INSTRUCTION(roll, n));
    LAUF_VERIFY(n < b->value_stack.cur_stack_size(), "roll", "invalid stack index");
}

void lauf_build_call(lauf_builder b, lauf_function_decl fn)
{
    b->update_location();

    b->bytecode.push_back(LAUF_VM_INSTRUCTION(call, bc_function_idx(fn._idx)));

    auto signature = b->functions[fn._idx].signature;
    b->value_stack.pop("call", signature.input_count);
    b->value_stack.push("call", signature.output_count);
}

void lauf_build_call_builtin(lauf_builder b, struct lauf_builtin fn)
{
    b->update_location();

    auto idx          = b->literals.insert(reinterpret_cast<void*>(fn.impl));
    auto stack_change = int32_t(fn.signature.input_count) - int32_t(fn.signature.output_count);
    b->bytecode.push_back(LAUF_VM_INSTRUCTION(call_builtin, stack_change, idx));

    b->value_stack.pop("call_builtin", fn.signature.input_count);
    b->value_stack.push("call_builtin", fn.signature.output_count);
}

void lauf_build_array_element(lauf_builder b, lauf_type type)
{
    b->update_location();

    b->bytecode.push_back(LAUF_VM_INSTRUCTION(array_element, type->layout.size));

    b->value_stack.pop("array_element", 2);
    b->value_stack.push("array_element");
}

void lauf_build_load_field(lauf_builder b, lauf_type type, size_t field)
{
    b->update_location();
    LAUF_VERIFY(field < type->field_count, "store_field", "invalid field count for type");

    auto idx = b->literals.insert(type);
    // If the last instruction is a store of the same field, turn it into a save instead.
    // TODO: invalid on jump
    if (!b->bytecode.empty() && b->bytecode.back() == LAUF_VM_INSTRUCTION(store_field, field, idx))
        b->bytecode.back().store_field.op = bc_op::save_field;
    else
        b->bytecode.push_back(LAUF_VM_INSTRUCTION(load_field, field, idx));

    b->value_stack.pop("load_field");
    b->value_stack.push("load_field");
}

void lauf_build_store_field(lauf_builder b, lauf_type type, size_t field)
{
    b->update_location();
    LAUF_VERIFY(field < type->field_count, "store_field", "invalid field count for type");

    auto idx = b->literals.insert(type);
    b->bytecode.push_back(LAUF_VM_INSTRUCTION(store_field, field, idx));

    b->value_stack.pop("store_field", 2);
}

void lauf_build_load_value(lauf_builder b, lauf_local_variable var)
{
    b->update_location();

    // If the last instruction is a store of the same address, turn it into a save instead.
    // TODO: invalid on jump
    if (!b->bytecode.empty() && b->bytecode.back() == LAUF_VM_INSTRUCTION(store_value, var._addr))
        b->bytecode.back().store_value.op = bc_op::save_value;
    else
        b->bytecode.push_back(LAUF_VM_INSTRUCTION(load_value, var._addr));
    b->value_stack.push("load_value");
}

void lauf_build_store_value(lauf_builder b, lauf_local_variable var)
{
    b->update_location();

    b->bytecode.push_back(LAUF_VM_INSTRUCTION(store_value, var._addr));
    b->value_stack.pop("store_value");
}

void lauf_build_panic(lauf_builder b)
{
    b->update_location();

    b->bytecode.push_back(LAUF_VM_INSTRUCTION(panic));
    b->value_stack.pop("panic");
}

void lauf_build_panic_if(lauf_builder b, lauf_condition condition)
{
    b->update_location();

    b->bytecode.push_back(LAUF_VM_INSTRUCTION(panic_if, translate_condition(condition)));
    b->value_stack.pop("panic_if", 2);
}

