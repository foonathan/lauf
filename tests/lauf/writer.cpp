// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/writer.hpp>

#include <cstdio>
#include <doctest/doctest.h>
#include <string>

namespace
{
constexpr auto test_path = "lauf_file_writer.delete-me";

std::string read_test_path()
{
    std::string result(1024, '\0');

    auto file = std::fopen(test_path, "rb");
    auto size = std::fread(result.data(), sizeof(char), result.size(), file);
    result.resize(size);
    std::fclose(file);

    return result;
}
} // namespace

TEST_CASE("string writer")
{
    SUBCASE("basic")
    {
        auto writer = lauf_create_string_writer();
        writer->write("abcdef", 3);
        writer->write("123");
        writer->format("%d", 42);

        CHECK(lauf_writer_get_string(writer) == std::string("abc12342"));

        lauf_destroy_writer(writer);
    }
    SUBCASE("long format")
    {
        auto writer = lauf_create_string_writer();
        writer->format("%*c", 1025, ' ');

        CHECK(lauf_writer_get_string(writer) == std::string(1025, ' '));

        lauf_destroy_writer(writer);
    }
}

TEST_CASE("file writer")
{
    std::remove(test_path);

    {
        auto writer = lauf_create_file_writer(test_path);
        writer->write("abcdef", 3);
        writer->write("123");
        writer->format("%d", 42);
        lauf_destroy_writer(writer);
    }
    CHECK(read_test_path() == "abc12342");

    std::remove(test_path);
}

