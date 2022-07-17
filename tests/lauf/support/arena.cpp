// Copyright (C) 2022 Jonathan Müller and null contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/support/arena.hpp>

#include <doctest/doctest.h>
#include <string>

TEST_CASE("arena")
{
    auto arena = lauf::arena::create();

    SUBCASE("basic")
    {
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
    }
    SUBCASE("clear and re-use")
    {
        auto a1 = arena->allocate(10 * 1024ull, 1);
        std::memset(a1, 'a', 10 * 1024ull);

        auto a2 = arena->allocate(10 * 1024ull, 1);
        std::memset(a2, 'b', 10 * 1024ull);

        arena->clear();

        auto b1 = arena->allocate(10 * 1024ull, 1);
        std::memset(b1, 'A', 10 * 1024ull);

        auto b2 = arena->allocate(10 * 1024ull, 1);
        std::memset(b2, 'B', 10 * 1024ull);

        CHECK(a1 == b1);
        CHECK(a2 == b2);

        for (auto i = 0u; i != 10 * 1024ull; ++i)
        {
            REQUIRE(static_cast<char*>(b1)[i] == 'A');
            REQUIRE(static_cast<char*>(b2)[i] == 'B');
        }
    }

    lauf::arena::destroy(arena);
}

