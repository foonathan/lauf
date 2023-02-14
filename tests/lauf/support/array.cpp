// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/support/array.hpp>

#include <doctest/doctest.h>

namespace
{
template <typename Array, typename... T>
void check_range(const Array& arr, const T... t)
{
    CHECK(!arr.empty());
    CHECK(arr.size() == sizeof...(T));

    auto cur = arr.begin();
    auto end = arr.end();
    ((REQUIRE(cur != end), CHECK(*cur == t), ++cur), ...);
}
} // namespace

TEST_CASE("array")
{
    lauf::array<int> array;
    CHECK(array.empty());
    CHECK(array.size() == 0);
    CHECK(array.begin() == array.end());

    SUBCASE("arena")
    {
        auto arena = lauf::arena::create();

        SUBCASE("single push_back")
        {
            array.push_back(*arena, 0);
            check_range(array, 0);

            array.push_back(*arena, 1);
            check_range(array, 0, 1);

            array.emplace_back(*arena, 2);
            check_range(array, 0, 1, 2);
        }
        SUBCASE("big push_back")
        {
            for (auto i = 0; i != 1024; ++i)
                array.push_back(*arena, 42);

            CHECK(array.size() == 1024);
            for (auto elem : array)
                CHECK(elem == 42);
        }
        SUBCASE("re-use after clear")
        {
            for (auto i = 0; i != 1024; ++i)
                array.push_back(*arena, 11);
            CHECK(array.size() == 1024);

            array.clear(*arena);

            for (auto i = 0; i != 2048; ++i)
                array.push_back(*arena, 42);

            CHECK(array.size() == 2048);
            for (auto elem : array)
                CHECK(elem == 42);
        }
        SUBCASE("re-use after arena clear")
        {
            for (auto i = 0; i != 1024; ++i)
                array.push_back(*arena, 11);
            CHECK(array.size() == 1024);

            array.clear(*arena);
            arena->clear();

            for (auto i = 0; i != 2048; ++i)
                array.push_back(*arena, 42);

            CHECK(array.size() == 2048);
            for (auto elem : array)
                CHECK(elem == 42);
        }

        lauf::arena::destroy(arena);
    }
    SUBCASE("page_allocator")
    {
        lauf::page_allocator alloc;

        SUBCASE("single push_back")
        {
            array.push_back(alloc, 0);
            check_range(array, 0);

            array.push_back(alloc, 1);
            check_range(array, 0, 1);

            array.emplace_back(alloc, 2);
            check_range(array, 0, 1, 2);
        }
        SUBCASE("big push_back")
        {
            for (auto i = 0; i != 10 * 1024; ++i)
                array.push_back(alloc, 42);

            CHECK(array.size() == 10 * 1024);
            for (auto elem : array)
                CHECK(elem == 42);
        }
        SUBCASE("re-use after clear")
        {
            for (auto i = 0; i != 10 * 1024; ++i)
                array.push_back(alloc, 11);
            CHECK(array.size() == 10 * 1024);

            array.clear(alloc);

            for (auto i = 0; i != 10 * 1024; ++i)
                array.push_back(alloc, 42);

            CHECK(array.size() == 10 * 1024);
            for (auto elem : array)
                CHECK(elem == 42);
        }
    }
}

