// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_SUPPORT_VIRTUAL_MEMORY_HPP_INCLUDED
#define SRC_LAUF_SUPPORT_VIRTUAL_MEMORY_HPP_INCLUDED

#include <cstddef>
#include <lauf/config.h>

namespace lauf
{
struct virtual_memory
{
    unsigned char* ptr  = nullptr;
    std::size_t    size = 0;
};

virtual_memory allocate_executable_memory(std::size_t page_count);
void           free_executable_memory(virtual_memory memory);

/// May change the address.
virtual_memory resize_executable_memory(virtual_memory memory, std::size_t new_page_count);

/// Enables writing to the executable memory, but disables execution.
void lock_executable_memory(virtual_memory memory);
/// Disables writing to the executable, but enables execution again.
void unlock_executable_memory(virtual_memory memory);
} // namespace lauf

#endif // SRC_LAUF_SUPPORT_VIRTUAL_MEMORY_HPP_INCLUDED

