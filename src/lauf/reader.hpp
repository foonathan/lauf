// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_READER_HPP_INCLUDED
#define SRC_LAUF_READER_HPP_INCLUDED

#include <lauf/reader.h>

#include <lexy/input/buffer.hpp>

struct lauf_reader
{
    lexy::buffer<lexy::utf8_encoding> buffer;
    const char*                       path = nullptr;
};

#endif // SRC_LAUF_READER_HPP_INCLUDED

