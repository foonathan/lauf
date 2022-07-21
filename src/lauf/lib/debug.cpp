// Copyright (C) 2022 Jonathan Müller and null contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib/debug.h>

#include <cinttypes>
#include <cstdio>
#include <lauf/asm/module.h>
#include <lauf/asm/type.h>
#include <lauf/runtime/builtin.h>
#include <lauf/runtime/process.h>
#include <lauf/runtime/stacktrace.h>
#include <lauf/runtime/value.h>

namespace
{
void debug_print(lauf_runtime_process* process, lauf_runtime_value value)
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
} // namespace

const lauf_runtime_builtin_function lauf_lib_debug_print
    = {[](lauf_runtime_process* process, const lauf_runtime_value* input, lauf_runtime_value*) {
           std::fprintf(stderr, "[lauf] debug print: ");
           debug_print(process, input[0]);
           std::fprintf(stderr, "\n");
           return true;
       },
       //
       1, 1, LAUF_RUNTIME_BUILTIN_NO_PANIC, "print", nullptr};

const lauf_runtime_builtin_function lauf_lib_debug_print_vstack
    = {[](lauf_runtime_process* process, const lauf_runtime_value* input, lauf_runtime_value*) {
           std::fprintf(stderr, "[lauf] value stack:\n");

           auto index = 0;
           for (auto cur = input; cur != lauf_runtime_get_vstack_base(process); ++cur)
           {
               std::fprintf(stderr, " %4d. ", index);
               debug_print(process, *cur);
               std::fprintf(stderr, "\n");
               ++index;
           }

           return true;
       },
       //
       0, 0, LAUF_RUNTIME_BUILTIN_NO_PANIC | LAUF_RUNTIME_BUILTIN_VM_ONLY, "print_vstack",
       &lauf_lib_debug_print};

const lauf_runtime_builtin_function lauf_lib_debug_print_cstack
    = {[](lauf_runtime_process* process, const lauf_runtime_value*, lauf_runtime_value*) {
           std::fprintf(stderr, "[lauf] call stack\n");
           auto index = 0;
           for (auto st = lauf_runtime_get_stacktrace(process); st != nullptr;
                st      = lauf_runtime_stacktrace_parent(st))
           {
               auto fn   = lauf_runtime_stacktrace_function(st);
               auto addr = lauf_asm_get_instruction_index(fn, lauf_runtime_stacktrace_address(st));

               std::fprintf(stderr, " %4d. %s\n", index, lauf_asm_function_name(fn));
               std::fprintf(stderr, "       at <%04lx>\n", addr);
               ++index;
           }
           return true;
       },
       //
       0, 0, LAUF_RUNTIME_BUILTIN_NO_PANIC, "print_cstack", &lauf_lib_debug_print_vstack};

const lauf_runtime_builtin_function lauf_lib_debug_break
    = {[](lauf_runtime_process*, const lauf_runtime_value*, lauf_runtime_value*) {
           __builtin_debugtrap();
           return true;
       },
       //
       0, 0, LAUF_RUNTIME_BUILTIN_NO_PROCESS, "break", &lauf_lib_debug_print_cstack};

const lauf_runtime_builtin_function lauf_lib_debug_read
    = {[](lauf_runtime_process*, const lauf_runtime_value*, lauf_runtime_value* output) {
           std::printf("[lauf] debug read: 0x");
           std::scanf("%" SCNx64, &output[0].as_uint);
           return true;
       },
       //
       0, 1, LAUF_RUNTIME_BUILTIN_NO_PROCESS, "read", &lauf_lib_debug_break};

const lauf_runtime_builtin_library lauf_lib_debug = {"lauf.debug", &lauf_lib_debug_read};

