// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/writer.hpp>

#include <cstdio>
#include <cstring>
#include <string>

void lauf_writer::write(const char* str)
{
    write(str, std::strlen(str));
}

[[gnu::format(printf, 2, 3)]] void lauf_writer::format(const char* fmt, ...)
{
    constexpr auto small_buffer = 1024;
    char           buffer[small_buffer + 1];

    va_list args;
    va_start(args, fmt);
    va_list copy;
    va_copy(copy, args);

    auto count = std::size_t(std::vsnprintf(buffer, small_buffer + 1, fmt, args));
    if (count > small_buffer)
    {
        std::string str(count + 1, '\0');
        std::vsnprintf(str.data(), str.size(), fmt, copy);
        write(str.data(), count);
    }
    else
    {
        write(buffer, count);
    }
}

void lauf_destroy_writer(lauf_writer* writer)
{
    delete writer;
}

//=== string writer ===//
namespace
{
struct string_writer final : lauf_writer
{
    std::string string;

    string_writer()
    {
        string.reserve(1024);
    }

    void write(const char* str, std::size_t size) override
    {
        string.append(str, size);
    }
};
} // namespace

lauf_writer* lauf_create_string_writer(void)
{
    return new string_writer();
}

const char* lauf_writer_get_string(lauf_writer* string_writer)
{
    return static_cast<struct string_writer*>(string_writer)->string.c_str();
}

//=== file writer ===//
namespace
{
struct file_writer final : lauf_writer
{
    std::FILE* file;

    explicit file_writer(const char* path)
    // We always open the file in binary mode.
    // The only text backend is dump, which is aimed at developers that should handle "wrong"
    // newlines.
    : file(std::fopen(path, "wb"))
    {}

    explicit file_writer(std::FILE* file) : file(file) {}

    ~file_writer()
    {
        if (file != stdout)
            std::fclose(file);
    }

    void write(const char* str, std::size_t size) override
    {
        std::fwrite(str, sizeof(char), size, file);
    }
};
} // namespace

lauf_writer* lauf_create_file_writer(const char* path)
{
    return new file_writer(path);
}

lauf_writer* lauf_create_stdout_writer(void)
{
    return new file_writer(stdout);
}

