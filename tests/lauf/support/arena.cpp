// Copyright (C) 2022 Jonathan Müller and null contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/support/arena.hpp>

#include <doctest/doctest.h>
#include <string>

TEST_CASE("arena")
{
    auto arena = lauf::arena::create();

    auto i   = arena->construct<int>(42);
    auto str = arena->strdup("Hello World!");

    auto fill_block = arena->allocate(10 * 1024ull, 1);
    std::memset(fill_block, 'a', 10 * 1024ull);

    auto next_block = arena->allocate(10 * 1024ull, 1);
    std::memset(next_block, 'b', 10 * 1024ull);

    auto extern_alloc = arena->allocate(100 * 1024ull, 1);
    std::memset(extern_alloc, 'c', 10 * 1024ull);

    auto str2 = arena->strdup("Goodbye!");

    REQUIRE(*i == 42);
    REQUIRE(str == std::string("Hello World!"));
    REQUIRE(str2 == std::string("Goodbye!"));

    for (auto i = 0u; i != 10 * 1024ull; ++i)
    {
        REQUIRE(static_cast<char*>(fill_block)[i] == 'a');
        REQUIRE(static_cast<char*>(next_block)[i] == 'b');
    }

    for (auto i = 0u; i != 10 * 1024ull; ++i)
        REQUIRE(static_cast<char*>(extern_alloc)[i] == 'c');

    lauf::arena::destroy(arena);
}

