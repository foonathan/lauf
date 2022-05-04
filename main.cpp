// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <climits>
#include <cstdio>

#include <lauf/builder.h>
#include <lauf/vm.h>

ptrdiff_t fn_increment(lauf_Value* stack_ptr)
{
    auto& top = stack_ptr[-1];
    top.as_int++;
    return 0;
}
const auto increment = lauf_builtin_function("increment", {1, 1}, &fn_increment);

int main()
{
    auto fn = [] {
        auto b = lauf_builder();
        lauf_builder_start_function(b, "test", lauf_FunctionSignature{0, 1});

        lauf_builder_push_int(b, 42);
        lauf_builder_call(b, increment);

        return lauf_builder_finish_function(b);
    }();

    lauf_Value output;
    lauf_vm_execute(lauf_vm(), fn, nullptr, &output);

    std::printf("result: %ld\n", output.as_int);
}

