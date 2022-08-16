// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/lib.h>

#include <lauf/runtime/builtin.h>

namespace
{
const lauf_runtime_builtin_library libs[]
    = {lauf_lib_debug, lauf_lib_heap, lauf_lib_int, lauf_lib_memory, lauf_lib_test};
}

const lauf_runtime_builtin_library* lauf_libs = libs;
const size_t lauf_libs_count                  = sizeof(libs) / sizeof(lauf_runtime_builtin_library);

