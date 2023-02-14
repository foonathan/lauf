// Copyright (C) 2022-2023 Jonathan Müller and null contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_WRITER_HPP_INCLUDED
#define SRC_LAUF_WRITER_HPP_INCLUDED

#include <lauf/writer.h>

struct lauf_writer
{
    lauf_writer()                              = default;
    lauf_writer(const lauf_writer&)            = delete;
    lauf_writer& operator=(const lauf_writer&) = delete;
    virtual ~lauf_writer()                     = default;

    virtual void write(const char* str, std::size_t size) = 0;

    void write(const char* str);

    [[gnu::format(printf, 2, 3)]] void format(const char* fmt, ...);
};

#endif // SRC_LAUF_WRITER_HPP_INCLUDED

