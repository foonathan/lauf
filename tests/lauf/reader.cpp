// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/reader.h>

#include <cstdio>
#include <doctest/doctest.h>
#include <lauf/reader.hpp>
#include <lauf/writer.hpp>
#include <string_view>

namespace
{
constexpr auto test_path = "lauf_file_writer.delete-me";

void write_test_file(const char* str)
{
    auto writer = lauf_create_file_writer(test_path);
    writer->write(str);
    lauf_destroy_writer(writer);
}
} // namespace

TEST_CASE("string reader")
{
    auto reader = lauf_create_string_reader("hello", 5);
    CHECK(std::string_view(reinterpret_cast<const char*>(reader->buffer.data()),
                           reader->buffer.size())
          == "hello");
    lauf_destroy_reader(reader);
}

TEST_CASE("file reader")
{
    std::remove(test_path);

    SUBCASE("non-existing")
    {
        CHECK(lauf_create_file_reader(test_path) == nullptr);
    }
    SUBCASE("existing")
    {
        write_test_file("hello");

        auto reader = lauf_create_file_reader(test_path);
        CHECK(reader);
        CHECK(std::string_view(reinterpret_cast<const char*>(reader->buffer.data()),
                               reader->buffer.size())
              == "hello");
        lauf_destroy_reader(reader);
    }

    std::remove(test_path);
}

