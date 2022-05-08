// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/frontend/text.h>

#include <cstring>
#include <lauf/builder.h>
#include <lauf/value.h>
#include <lexy/action/parse.hpp>
#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <lexy/input/string_input.hpp>
#include <lexy_ext/report_error.hpp>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

struct lauf_frontend_text_parser_impl
{
    std::vector<lauf_module>                 modules;
    std::map<std::string_view, lauf_builtin> builtins;
    std::map<std::string_view, lauf_type>    types;
    std::set<std::string>                    names;
};

namespace
{
struct block_terminator
{
    enum kind
    {
        return_,
        jump,
        branch,
    } kind;
    std::string_view if_true, if_false;
    lauf_condition   condition;
};
} // namespace

namespace lauf::text_grammar
{
namespace dsl = lexy::dsl;

auto identifier = dsl::identifier(dsl::ascii::alpha_underscore, dsl::ascii::alpha_digit_underscore);

struct local_label
{
    static constexpr auto rule  = dsl::percent_sign >> identifier;
    static constexpr auto value = lexy::as_string<std::string_view>;
};

struct global_label
{
    static constexpr auto rule  = dsl::at_sign >> identifier;
    static constexpr auto value = lexy::as_string<std::string_view>;
};

//=== instructions ===//
struct inst_int
{
    static constexpr auto rule  = LEXY_KEYWORD("int", identifier) >> dsl::integer<lauf_value_int>;
    static constexpr auto build = &lauf_build_int;
};
struct inst_local_addr
{
    static constexpr auto rule = LEXY_KEYWORD("local_addr", identifier) >> dsl::p<local_label>;
};

struct inst_drop
{
    static constexpr auto rule  = LEXY_KEYWORD("drop", identifier) >> dsl::integer<size_t>;
    static constexpr auto build = &lauf_build_drop;
};
struct inst_pick
{
    static constexpr auto rule  = LEXY_KEYWORD("pick", identifier) >> dsl::integer<size_t>;
    static constexpr auto build = &lauf_build_pick;
};
struct inst_roll
{
    static constexpr auto rule  = LEXY_KEYWORD("roll", identifier) >> dsl::integer<size_t>;
    static constexpr auto build = &lauf_build_roll;
};

struct inst_recurse
{
    static constexpr auto rule  = LEXY_KEYWORD("recurse", identifier);
    static constexpr auto build = &lauf_build_recurse;
};
struct inst_call
{
    static constexpr auto rule = LEXY_KEYWORD("call", identifier) >> dsl::p<global_label>;
};
struct inst_call_builtin
{
    static constexpr auto rule = LEXY_KEYWORD("call_builtin", identifier) >> dsl::p<global_label>;
};

struct inst_load_field
{
    static constexpr auto rule = LEXY_KEYWORD("load_field", identifier)
                                 >> dsl::p<global_label> + dsl::period + dsl::integer<size_t>;
};
struct inst_store_field
{
    static constexpr auto rule = LEXY_KEYWORD("store_field", identifier)
                                 >> dsl::p<global_label> + dsl::period + dsl::integer<size_t>;
};

struct inst
{
    static constexpr auto rule
        = (dsl::p<inst_int> | dsl::p<inst_local_addr>                             //
           | dsl::p<inst_drop> | dsl::p<inst_pick> | dsl::p<inst_roll>            //
           | dsl::p<inst_recurse> | dsl::p<inst_call> | dsl::p<inst_call_builtin> //
           | dsl::p<inst_load_field> | dsl::p<inst_store_field>)                  //
        +dsl::semicolon;
    static constexpr auto value = lexy::forward<void>;
};

//=== block terminator ===//
struct term_return
{
    static constexpr auto rule = LEXY_KEYWORD("return", identifier);
    static constexpr auto value
        = lexy::bind(lexy::construct<block_terminator>, block_terminator::return_);
};

struct term_jump
{
    static constexpr auto rule = LEXY_KEYWORD("jump", identifier) >> dsl::p<local_label>;
    static constexpr auto value
        = lexy::bind(lexy::construct<block_terminator>, block_terminator::jump, lexy::_1);
};

struct term_branch
{
    static constexpr auto ccs
        = lexy::symbol_table<lauf_condition>.map<LEXY_SYMBOL("if_true")>(LAUF_IF_TRUE).map<LEXY_SYMBOL("if_false")>(LAUF_IF_FALSE);

    static constexpr auto rule = LEXY_KEYWORD("branch", identifier)
                                 >> dsl::symbol<ccs> + dsl::p<local_label> //
                                        + LEXY_KEYWORD("else", identifier) + dsl::p<local_label>;
    static constexpr auto value
        = lexy::bind(lexy::construct<block_terminator>, block_terminator::branch, lexy::_2,
                     lexy::_3, lexy::_1);
};

struct term
{
    static constexpr auto rule
        = (dsl::p<term_return> | dsl::p<term_jump> | dsl::p<term_branch>) >> dsl::semicolon;
    static constexpr auto value = lexy::forward<block_terminator>;
};

//=== function and block ===//
struct signature
{
    static constexpr auto rule
        = dsl::parenthesized(dsl::times<2>(dsl::integer<uint8_t>, dsl::sep(LEXY_LIT("=>"))));
    static constexpr auto value = lexy::construct<lauf_signature>;
};

struct block
{
    struct header
    {
        static constexpr auto rule
            = LEXY_KEYWORD("block", identifier) >> dsl::p<local_label> + dsl::p<signature>;
    };

    static constexpr auto rule = dsl::p<header> >> dsl::curly_bracketed(
                                     dsl::terminator(dsl::p<term>).opt_list(dsl::p<inst>));
};

struct local
{
    static constexpr auto rule
        = LEXY_KEYWORD("local", identifier)
          >> dsl::p<local_label> + dsl::colon + dsl::p<global_label> + dsl::semicolon;
};

struct function
{
    struct header
    {
        static constexpr auto rule
            = LEXY_KEYWORD("function", identifier) >> dsl::p<global_label> + dsl::p<signature>;
    };

    static constexpr auto rule
        = dsl::p<header> >> (dsl::curly_bracketed.list(dsl::p<block> | dsl::p<local>)
                             | dsl::semicolon >> dsl::nullopt);
};

//=== module ===//
struct module_
{
    struct header
    {
        static constexpr auto rule
            = LEXY_KEYWORD("module", identifier) >> dsl::p<global_label> + dsl::semicolon;
    };

    static constexpr auto name = "module";
    static constexpr auto whitespace
        = dsl::ascii::space | LEXY_LIT("#") >> dsl::until(dsl::newline).or_eof();

    static constexpr auto rule = dsl::p<header> >> dsl::while_(dsl::p<function>) + dsl::eof;
};
} // namespace lauf::text_grammar

namespace
{
struct partial_block
{
    lauf_block_builder              builder;
    std::optional<block_terminator> term;
};

struct builder
{
    lauf_frontend_text_parser parser;

    mutable lauf_module_builder mod = nullptr;

    mutable lauf_function_builder                             cur_function = nullptr;
    mutable std::map<std::string_view, lauf_function_builder> function_labels;

    mutable std::size_t                                     cur_block = 0;
    mutable std::vector<partial_block>                      blocks;
    mutable std::map<std::string_view, lauf_block_builder>  block_labels;
    mutable std::map<std::string_view, lauf_local_variable> local_labels;

    auto value_of(lauf::text_grammar::module_::header) const
    {
        return lexy::callback([&](std::string_view name) {
            auto result = parser->names.emplace(name);
            mod         = lauf_build_module(result.first->c_str());
        });
    }
    auto value_of(lauf::text_grammar::module_) const
    {
        return lexy::callback<lauf_module>([&] {
            auto result = lauf_finish_module(mod);
            parser->modules.push_back(result);
            return result;
        });
    }

    auto value_of(lauf::text_grammar::function::header) const
    {
        return lexy::callback([&](std::string_view name, lauf_signature sig) {
            auto iter = function_labels.find(name);
            if (iter != function_labels.end())
            {
                cur_function = iter->second;
            }
            else
            {
                auto result           = parser->names.emplace(name);
                cur_function          = lauf_build_function(mod, result.first->c_str(), sig);
                function_labels[name] = cur_function;
            }
        });
    }
    auto value_of(lauf::text_grammar::function) const
    {
        return lexy::noop >> lexy::callback(
                   [&](lexy::nullopt) {
                       // Forward declaration, do nothing.
                   },
                   [&] {
                       for (auto& block : blocks)
                           switch (block.term->kind)
                           {
                           case block_terminator::return_:
                               lauf_finish_block_return(block.builder);
                               break;
                           case block_terminator::jump:
                               lauf_finish_block_jump(block.builder,
                                                      block_labels.at(block.term->if_true));
                               break;
                           case block_terminator::branch:
                               lauf_finish_block_branch(block.builder, block.term->condition,
                                                        block_labels.at(block.term->if_true),
                                                        block_labels.at(block.term->if_false));
                               break;
                           }

                       lauf_finish_function(cur_function);
                       cur_function = nullptr;
                       cur_block    = 0;
                       blocks.clear();
                       block_labels.clear();
                       local_labels.clear();
                   });
    }

    auto value_of(lauf::text_grammar::block::header) const
    {
        return lexy::callback([&](std::string_view name, lauf_signature sig) {
            auto block         = lauf_build_block(cur_function, sig);
            block_labels[name] = block;

            cur_block = blocks.size();
            blocks.push_back(partial_block{block});
        });
    }
    auto value_of(lauf::text_grammar::block) const
    {
        // TODO: simplify on lexy's side
        return lexy::noop
               >> lexy::callback([&](lexy::nullopt,
                                     block_terminator term) { blocks[cur_block].term = term; },
                                 [&](block_terminator term) { blocks[cur_block].term = term; });
    }

    auto value_of(lauf::text_grammar::local) const
    {
        return lexy::callback([&](std::string_view name, std::string_view type) {
            local_labels[name]
                = lauf_build_local_variable(cur_function, parser->types.at(type)->layout);
        });
    }

    template <typename Inst, typename = decltype(Inst::build)>
    auto value_of(Inst) const
    {
        return lexy::callback(
            [&](const auto&... args) { Inst::build(blocks[cur_block].builder, args...); });
    }
    auto value_of(lauf::text_grammar::inst_local_addr) const
    {
        return lexy::callback([&](std::string_view name) {
            lauf_build_local_addr(blocks[cur_block].builder, local_labels.at(name));
        });
    }
    auto value_of(lauf::text_grammar::inst_call) const
    {
        return lexy::callback([&](std::string_view fn) {
            lauf_build_call(blocks[cur_block].builder, function_labels.at(fn));
        });
    }
    auto value_of(lauf::text_grammar::inst_call_builtin) const
    {
        return lexy::callback([&](std::string_view builtin) {
            lauf_build_call_builtin(blocks[cur_block].builder, parser->builtins.at(builtin));
        });
    }
    auto value_of(lauf::text_grammar::inst_load_field) const
    {
        return lexy::callback([&](std::string_view type, std::size_t field) {
            lauf_build_load_field(blocks[cur_block].builder, parser->types.at(type), field);
        });
    }
    auto value_of(lauf::text_grammar::inst_store_field) const
    {
        return lexy::callback([&](std::string_view type, std::size_t field) {
            lauf_build_store_field(blocks[cur_block].builder, parser->types.at(type), field);
        });
    }
};
} // namespace

lauf_frontend_text_parser lauf_frontend_text_create_parser(void)
{
    return new lauf_frontend_text_parser_impl{};
}

void lauf_frontend_text_register_builtin(lauf_frontend_text_parser p, const char* name,
                                         lauf_builtin builtin)
{
    p->builtins[name] = builtin;
}

void lauf_frontend_text_register_type(lauf_frontend_text_parser p, const char* name, lauf_type type)
{
    p->types[name] = type;
}

lauf_module lauf_frontend_text(lauf_frontend_text_parser p, const char* data, size_t size)
{
    auto input = lexy::string_input<lexy::ascii_encoding>(data, size);
    auto result
        = lexy::parse<lauf::text_grammar::module_>(input, builder{p}, lexy_ext::report_error);
    return result ? result.value() : nullptr;
}

lauf_module lauf_frontend_text_cstr(lauf_frontend_text_parser p, const char* str)
{
    return lauf_frontend_text(p, str, std::strlen(str));
}

void lauf_frontend_text_destroy_parser(lauf_frontend_text_parser p)
{
    for (auto mod : p->modules)
        lauf_module_destroy(mod);
    delete p;
}

