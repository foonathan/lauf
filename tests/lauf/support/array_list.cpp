// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/support/array_list.hpp>

#include <doctest/doctest.h>

namespace
{
template <typename List, typename... T>
void check_range(const List& list, const T... t)
{
    CHECK(!list.empty());
    CHECK(list.size() == sizeof...(T));

    auto begin = list.begin();
    auto end   = list.end();

    auto cur = begin;
    ((REQUIRE(cur != end), CHECK(*cur == t), ++cur), ...);

    for (auto i = 0u; i != sizeof...(t); ++i)
    {
        REQUIRE(cur != begin);
        --cur;
    }
    REQUIRE(cur == begin);
}
} // namespace

TEST_CASE("array_list")
{
    auto arena = lauf::arena::create();

    SUBCASE("int")
    {
        lauf::array_list<int> list;
        CHECK(list.empty());
        CHECK(list.size() == 0);
        CHECK(list.begin() == list.end());

        SUBCASE("single push_back")
        {
            list.push_back(*arena, 0);
            check_range(list, 0);

            list.push_back(*arena, 1);
            check_range(list, 0, 1);

            list.emplace_back(*arena, 2);
            check_range(list, 0, 1, 2);

            list.pop_back();
            check_range(list, 0, 1);

            list.push_back(*arena, 3);
            check_range(list, 0, 1, 3);
        }
        SUBCASE("big push_back")
        {
            for (auto i = 0; i != 1024; ++i)
                list.push_back(*arena, 42);

            SUBCASE("pop_back")
            {
                for (auto i = 0; i != 1024; ++i)
                    list.pop_back();
                CHECK(list.size() == 0);

                for (auto i = 0; i != 1024; ++i)
                    list.push_back(*arena, 42);
            }

            CHECK(list.size() == 1024);
            for (auto elem : list)
                CHECK(elem == 42);
        }

        SUBCASE("re-use after reset")
        {
            for (auto i = 0; i != 1024; ++i)
                list.push_back(*arena, 11);
            CHECK(list.size() == 1024);

            list.reset();

            for (auto i = 0; i != 2048; ++i)
                list.push_back(*arena, 42);

            CHECK(list.size() == 2048);
            for (auto elem : list)
                CHECK(elem == 42);
        }
        SUBCASE("re-use after arena clear")
        {
            for (auto i = 0; i != 1024; ++i)
                list.push_back(*arena, 11);
            CHECK(list.size() == 1024);

            list.reset();
            arena->clear();

            for (auto i = 0; i != 2048; ++i)
                list.push_back(*arena, 42);

            CHECK(list.size() == 2048);
            for (auto elem : list)
                CHECK(elem == 42);
        }
    }
    SUBCASE("nested")
    {
        lauf::array_list<lauf::array_list<int>> list;
        CHECK(list.empty());
        CHECK(list.size() == 0);
        CHECK(list.begin() == list.end());

        auto& inner0 = list.emplace_back(*arena);
        inner0.push_back(*arena, 1);
        inner0.push_back(*arena, 2);
        inner0.push_back(*arena, 3);

        auto& inner1 = list.emplace_back(*arena);
        inner1.push_back(*arena, 42);
        inner1.push_back(*arena, 11);

        auto idx = 0;
        for (auto& inner : list)
        {
            if (idx == 0)
                check_range(inner, 1, 2, 3);
            else
                check_range(inner, 42, 11);
            ++idx;
        }
    }

    lauf::arena::destroy(arena);
}

