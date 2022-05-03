// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <cstdio>

#include <lauf/builder.h>
#include <lauf/vm.h>

int main()
{
    auto fn = [] {
        auto b = lauf_builder();
        lauf_builder_start_function(b, "test", lauf_FunctionSignature{0, 1});

        auto v42 = lauf_builder_push_int(b, 42);
        auto v11 = lauf_builder_push_int(b, 11);
        lauf_builder_pop(b, v11);

        return lauf_builder_finish_function(b);
    }();

    lauf_Value output;
    lauf_vm_execute(lauf_vm(), fn, nullptr, &output);

    std::printf("result: %ld\n", output.as_int);
}

