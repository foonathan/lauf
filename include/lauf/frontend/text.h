// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_FRONTEND_TEXT_H_INCLUDED
#define LAUF_FRONTEND_TEXT_H_INCLUDED

#include <lauf/builtin.h>
#include <lauf/config.h>
#include <lauf/module.h>

LAUF_HEADER_START

typedef struct lauf_frontend_text_parser_impl* lauf_frontend_text_parser;

lauf_frontend_text_parser lauf_frontend_text_create_parser(void);

void lauf_frontend_text_register_builtin(lauf_frontend_text_parser p, const char* name,
                                         lauf_builtin builtin);

/// Parses a module from text; `data` can be freed later.
lauf_module lauf_frontend_text(lauf_frontend_text_parser p, const char* data, size_t size);
lauf_module lauf_frontend_text_cstr(lauf_frontend_text_parser p, const char* str);

/// Frees the parser and all modules parsed with it.
void lauf_frontend_text_destroy_parser(lauf_frontend_text_parser p);

LAUF_HEADER_END

#endif // LAUF_FRONTEND_TEXT_H_INCLUDED

