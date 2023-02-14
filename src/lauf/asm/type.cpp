// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/asm/type.h>

#include <lauf/runtime/builtin.h>
#include <lauf/runtime/value.h>
#include <lauf/support/align.hpp>

lauf_asm_layout lauf_asm_array_layout(lauf_asm_layout element_layout, size_t element_count)
{
    element_layout.size
        = lauf::round_to_multiple_of_alignment(element_layout.size, element_layout.alignment);
    element_layout.size *= element_count;
    return element_layout;
}

lauf_asm_layout lauf_asm_aggregate_layout(const lauf_asm_layout* member_layouts,
                                          size_t                 member_count)
{
    auto size      = std::size_t(0);
    auto alignment = std::size_t(1);
    for (auto i = 0u; i != member_count; ++i)
    {
        size += lauf::align_offset(size, member_layouts[i].alignment);
        size += member_layouts[i].size;

        if (member_layouts[i].alignment > alignment)
            alignment = member_layouts[i].alignment;
    }
    return {size, alignment};
}

namespace
{
LAUF_RUNTIME_BUILTIN_IMPL bool load_value(const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr,
                                          lauf_runtime_stack_frame* frame_ptr,
                                          lauf_runtime_process*     process)
{
    vstack_ptr[1] = *static_cast<const lauf_runtime_value*>(vstack_ptr[1].as_native_ptr);
    ++vstack_ptr;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN_IMPL bool store_value(const lauf_asm_inst* ip, lauf_runtime_value* vstack_ptr,
                                           lauf_runtime_stack_frame* frame_ptr,
                                           lauf_runtime_process*     process)
{
    *static_cast<lauf_runtime_value*>(vstack_ptr[1].as_native_ptr) = vstack_ptr[2];
    vstack_ptr += 3;

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}
} // namespace

const lauf_asm_type lauf_asm_type_value = {LAUF_ASM_NATIVE_LAYOUT_OF(lauf_runtime_value),
                                           1,
                                           &load_value,
                                           &store_value,
                                           "lauf.Value",
                                           nullptr};

