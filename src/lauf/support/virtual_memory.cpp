// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/support/virtual_memory.hpp>

#include <sys/mman.h>
#include <unistd.h>

namespace
{
const auto page_size = ::sysconf(_SC_PAGE_SIZE);
}

lauf::virtual_memory lauf::allocate_executable_memory(std::size_t page_count)
{
    auto pages = ::mmap(nullptr, page_count * page_size, PROT_READ | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pages == nullptr)
        return {nullptr, 0};

    return {static_cast<unsigned char*>(pages), page_count * page_size};
}

void lauf::free_executable_memory(virtual_memory memory)
{
    if (memory.ptr != nullptr)
        ::munmap(memory.ptr, memory.size);
}

lauf::virtual_memory lauf::resize_executable_memory(virtual_memory memory,
                                                    std::size_t    new_page_count)
{
    if (memory.ptr == nullptr)
        return allocate_executable_memory(new_page_count);

    auto pages = ::mremap(memory.ptr, memory.size, new_page_count * page_size, MREMAP_MAYMOVE);
    if (pages == nullptr)
        return {nullptr, 0};

    return {static_cast<unsigned char*>(pages), new_page_count * page_size};
}

void lauf::lock_executable_memory(virtual_memory memory)
{
    ::mprotect(memory.ptr, memory.size, PROT_READ | PROT_WRITE);
}

void lauf::unlock_executable_memory(virtual_memory memory)
{
    ::mprotect(memory.ptr, memory.size, PROT_READ | PROT_EXEC);
}

