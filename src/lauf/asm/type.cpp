// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/asm/type.h>

#include <lauf/runtime/builtin.h>
#include <lauf/runtime/value.h>

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

const lauf_asm_type lauf_asm_type_value
    = {LAUF_ASM_NATIVE_LAYOUT_OF(lauf_runtime_value), 1, &load_value, &store_value, "lauf.Value"};

