// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <cassert>
#include <cstdio>
#include <lauf/asm/builder.h>
#include <lauf/asm/module.h>
#include <lauf/asm/program.h>
#include <lauf/lib/int.h>
#include <lauf/runtime/builtin.h>
#include <lauf/runtime/value.h>
#include <lauf/vm.h>
#include <string_view>

#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#define LAUF_BENCHMARK(Name)                                                                       \
    do                                                                                             \
    {                                                                                              \
        if (selected.empty() || selected == #Name)                                                 \
            benchmark(#Name, &(Name));                                                             \
    } while (0)

void bm_constant(lauf_asm_builder* b)
{
    lauf_asm_inst_sint(b, 42);
    lauf_asm_inst_return(b);
}

void bm_add(lauf_asm_builder* b)
{
    lauf_asm_inst_sint(b, 42);
    lauf_asm_inst_sint(b, 11);
    lauf_asm_inst_call_builtin(b, lauf_lib_int_sadd(LAUF_LIB_INT_OVERFLOW_PANIC));
    lauf_asm_inst_return(b);
}

void bm_multiply(lauf_asm_builder* b)
{
    lauf_asm_inst_sint(b, 4);
    lauf_asm_inst_sint(b, 1024);
    lauf_asm_inst_call_builtin(b, lauf_lib_int_smul(LAUF_LIB_INT_OVERFLOW_PANIC));
    lauf_asm_inst_sint(b, 1024);
    lauf_asm_inst_call_builtin(b, lauf_lib_int_smul(LAUF_LIB_INT_OVERFLOW_PANIC));
    lauf_asm_inst_return(b);
}

int main(int argc, char* argv[])
{
    auto vm      = lauf_create_vm(lauf_default_vm_options);
    auto mod     = lauf_asm_create_module("benchmark");
    auto chunk   = lauf_asm_create_chunk(mod);
    auto builder = lauf_asm_create_builder(lauf_asm_default_build_options);

    ankerl::nanobench::Bench b;
    b.minEpochTime(std::chrono::milliseconds(500));
    auto benchmark = [&](const char* name, auto fn) {
        auto runner = [&] {
            lauf_asm_build_chunk(builder, mod, chunk, {0, 1});
            fn(builder);
            lauf_asm_build_finish(builder);
            auto program = lauf_asm_create_program_from_chunk(mod, chunk);

            lauf_runtime_value result;
            auto               panic = lauf_vm_execute_oneshot(vm, program, nullptr, &result);
            ankerl::nanobench::doNotOptimizeAway(panic);
            ankerl::nanobench::doNotOptimizeAway(result);
        };
        b.run(name, runner);
    };

    auto selected = argc == 2 ? std::string_view(argv[1]) : "";

    LAUF_BENCHMARK(bm_constant);
    LAUF_BENCHMARK(bm_add);
    LAUF_BENCHMARK(bm_multiply);

    lauf_asm_destroy_builder(builder);
    lauf_asm_destroy_module(mod);
    lauf_destroy_vm(vm);
}

