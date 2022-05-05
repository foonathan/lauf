// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <climits>
#include <cstdio>

#include <lauf/builder.h>
#include <lauf/vm.h>

LAUF_BUILTIN_UNARY_OP(increment)
{
    value.as_int++;
    return value;
}

LAUF_BUILTIN_BINARY_OP(add)
{
    lhs.as_int += rhs.as_int;
    return lhs;
}

int main()
{
    auto fn = [] {
        auto b = lauf_builder();
        lauf_builder_function(b, "test", {1, 1});

        lauf_BuilderIf if_;
        lauf_builder_argument(b, 0);
        lauf_builder_if(b, &if_, LAUF_IF_NONZERO);
        lauf_builder_int(b, 42);
        lauf_builder_call_builtin(b, increment);
        lauf_builder_return(b);
        lauf_builder_end_if(b, &if_);

        lauf_builder_int(b, 11);
        lauf_builder_return(b);

        return lauf_builder_end_function(b);
    }();

    auto vm = lauf_vm(lauf_default_vm_options);

    lauf_Value input = {.as_int = 0};
    lauf_Value output;
    lauf_vm_execute(vm, fn, &input, &output);
    std::printf("result: %ld\n", output.as_int);

    lauf_vm_destroy(vm);
}

