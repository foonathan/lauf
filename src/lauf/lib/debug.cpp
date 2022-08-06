// Copyright (C) 2022 Jonathan Müller and null contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib/debug.hpp>

#include <cinttypes>
#include <cstdio>
#include <lauf/asm/module.h>
#include <lauf/asm/type.h>
#include <lauf/runtime/builtin.h>
#include <lauf/runtime/process.h>
#include <lauf/runtime/stacktrace.h>
#include <lauf/runtime/value.h>

void lauf::debug_print(lauf_runtime_process* process, lauf_runtime_value value)
{
    std::fprintf(stderr, "0x%" PRIX64, value.as_uint);
    std::fprintf(stderr, " (uint = %" PRIu64 ", sint = %" PRId64, value.as_uint, value.as_sint);

    if (auto ptr = lauf_runtime_get_const_ptr(process, value.as_address, {0, 1}))
        std::fprintf(stderr, ", address = %p", ptr);
    else if (value.as_uint == lauf_uint(-1))
        std::fprintf(stderr, ", address = NULL");

    if (auto fn = lauf_runtime_get_function_ptr_any(process, value.as_function_address))
        std::fprintf(stderr, ", function = @'%s'", lauf_asm_function_name(fn));
    else if (value.as_uint == lauf_uint(-1))
        std::fprintf(stderr, ", function = NULL");

    std::fprintf(stderr, ")");
}

void lauf::debug_print_cstack(lauf_runtime_process* process)
{
    auto index = 0;
    for (auto st = lauf_runtime_get_stacktrace(process); st != nullptr;
         st      = lauf_runtime_stacktrace_parent(st))
    {
        auto fn   = lauf_runtime_stacktrace_function(st);
        auto addr = lauf_asm_get_instruction_index(fn, lauf_runtime_stacktrace_instruction(st));

        std::fprintf(stderr, " %4d. %s\n", index, lauf_asm_function_name(fn));
        std::fprintf(stderr, "       at <%04lx>\n", addr);
        ++index;
    }
}

LAUF_RUNTIME_BUILTIN(lauf_lib_debug_print, 1, 1, LAUF_RUNTIME_BUILTIN_NO_PANIC, "print", nullptr)
{
    std::fprintf(stderr, "[lauf] debug print: ");
    lauf::debug_print(process, vstack_ptr[0]);
    std::fprintf(stderr, "\n");

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_debug_print_vstack, 0, 0,
                     LAUF_RUNTIME_BUILTIN_NO_PANIC | LAUF_RUNTIME_BUILTIN_VM_ONLY, "print_vstack",
                     &lauf_lib_debug_print)
{
    std::fprintf(stderr, "[lauf] value stack:\n");

    auto index = 0;
    for (auto cur = vstack_ptr; cur != lauf_runtime_get_vstack_base(process); ++cur)
    {
        std::fprintf(stderr, " %4d. ", index);
        lauf::debug_print(process, *cur);
        std::fprintf(stderr, "\n");
        ++index;
    }

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_debug_print_cstack, 0, 0, LAUF_RUNTIME_BUILTIN_NO_PANIC,
                     "print_cstack", &lauf_lib_debug_print_vstack)
{
    std::fprintf(stderr, "[lauf] call stack\n");
    lauf::debug_print_cstack(process);
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_debug_break, 0, 0,
                     LAUF_RUNTIME_BUILTIN_NO_PROCESS | LAUF_RUNTIME_BUILTIN_NO_PANIC, "break",
                     &lauf_lib_debug_print_cstack)
{
    __builtin_debugtrap();
    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

LAUF_RUNTIME_BUILTIN(lauf_lib_debug_read, 0, 1,
                     LAUF_RUNTIME_BUILTIN_NO_PROCESS | LAUF_RUNTIME_BUILTIN_NO_PANIC, "read",
                     &lauf_lib_debug_break)
{
    std::printf("[lauf] debug read: 0x");

    --vstack_ptr;
    std::scanf("%" SCNx64, &vstack_ptr->as_uint);

    LAUF_RUNTIME_BUILTIN_DISPATCH;
}

const lauf_runtime_builtin_library lauf_lib_debug = {"lauf.debug", &lauf_lib_debug_read};

