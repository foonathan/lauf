// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/type.h>

#include <new>

namespace
{
lauf_value load_value(const void* object_address, size_t)
{
    return *static_cast<const lauf_value*>(object_address);
}

void store_value(void* object_address, size_t, lauf_value value)
{
    ::new (object_address) lauf_value(value);
}
} // namespace

const lauf_type_data lauf_value_type
    = {LAUF_NATIVE_LAYOUT_OF(lauf_value), 1, load_value, store_value};
