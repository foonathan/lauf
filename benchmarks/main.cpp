// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <cassert>
#include <cstdio>
#include <lauf/builder.h>
#include <lauf/program.h>
#include <lauf/vm.h>
#include <string_view>

#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#define LAUF_BENCHMARK(Name)                                                                       \
    do                                                                                             \
    {                                                                                              \
        if (selected.empty() || selected == #Name)                                                 \
        {                                                                                          \
            extern lauf_program Name(lauf_builder);                                                \
            benchmark(#Name, &(Name));                                                             \
        }                                                                                          \
    } while (0)

int main(int argc, char* argv[])
{
    auto vm      = lauf_vm_create(lauf_default_vm_options);
    auto builder = lauf_builder_create();

    ankerl::nanobench::Bench b;
    b.minEpochTime(std::chrono::milliseconds(500));
    auto benchmark = [&](const char* name, auto fn) {
        auto runner = [&] {
            auto program = fn(builder);

            lauf_value result;
            auto       panic = lauf_vm_execute(vm, program, nullptr, &result);
            ankerl::nanobench::doNotOptimizeAway(panic);
            ankerl::nanobench::doNotOptimizeAway(result);

            auto mod = lauf_function_get_module(lauf_program_get_entry_function(program));
            lauf_program_destroy(program);
            lauf_module_destroy(mod);
        };
        b.run(name, runner);
    };

    auto selected = argc == 2 ? std::string_view(argv[1]) : "";

    LAUF_BENCHMARK(trivial_add);
    LAUF_BENCHMARK(trivial_multiply);
    LAUF_BENCHMARK(recursive_fib);
    LAUF_BENCHMARK(recursive_fib_memory);
    LAUF_BENCHMARK(iterative_fib);

    lauf_vm_destroy(vm);
    lauf_builder_destroy(builder);
}

