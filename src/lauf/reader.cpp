// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/reader.hpp>

#include <lexy/input/file.hpp>

void lauf_destroy_reader(lauf_reader* reader)
{
    delete reader;
}

lauf_reader* lauf_create_string_reader(const char* str, std::size_t size)
{
    return new lauf_reader{lexy::buffer<lexy::utf8_encoding>(str, size)};
}

lauf_reader* lauf_create_cstring_reader(const char* str)
{
    return lauf_create_string_reader(str, std::strlen(str));
}

lauf_reader* lauf_create_file_reader(const char* path)
{
    auto result = lexy::read_file<lexy::utf8_encoding>(path);
    if (!result)
        return nullptr;

    return new lauf_reader{LEXY_MOV(result).buffer()};
}

lauf_reader* lauf_create_stdin_reader(void)
{
    auto result = lexy::read_stdin<lexy::utf8_encoding>();
    if (!result)
        return nullptr;

    return new lauf_reader{LEXY_MOV(result).buffer()};
}
