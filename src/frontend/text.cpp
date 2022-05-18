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
#include <lexy/input_location.hpp>
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
    std::set<std::string>                    data;

    const char* intern(std::string&& str)
    {
        return data.emplace(std::move(str)).first->c_str();
    }
    const char* intern(std::string_view str)
    {
        return intern(std::string(str));
    }
    const char* intern(const char* str)
    {
        return intern(std::string(str));
    }
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

struct ref_local_var
{
    static constexpr auto rule = dsl::p<local_label>;
};
struct ref_function
{
    static constexpr auto rule = dsl::p<global_label>;
};
struct ref_data
{
    static constexpr auto rule = dsl::p<global_label>;
};
struct ref_builtin
{
    static constexpr auto rule = dsl::p<global_label>;
};
struct ref_type
{
    static constexpr auto rule = dsl::p<global_label>;
};

static constexpr auto ccs = lexy::symbol_table<lauf_condition> //
                                .map<LEXY_SYMBOL("is_true")>(LAUF_IS_TRUE)
                                .map<LEXY_SYMBOL("is_false")>(LAUF_IS_FALSE)
                                .map<LEXY_SYMBOL("cmp_eq")>(LAUF_CMP_EQ)
                                .map<LEXY_SYMBOL("cmp_ne")>(LAUF_CMP_NE)
                                .map<LEXY_SYMBOL("cmp_lt")>(LAUF_CMP_LT)
                                .map<LEXY_SYMBOL("cmp_le")>(LAUF_CMP_LE)
                                .map<LEXY_SYMBOL("cmp_gt")>(LAUF_CMP_GT)
                                .map<LEXY_SYMBOL("cmp_ge")>(LAUF_CMP_GE);

struct debug_location
{
    static constexpr auto rule = dsl::position;
};

//=== instructions ===//
struct inst_int
{
    static constexpr auto rule  = LEXY_KEYWORD("int", identifier) >> dsl::integer<lauf_value_sint>;
    static constexpr auto build = &lauf_build_int;
};
struct inst_ptr
{
    static constexpr auto rule  = LEXY_KEYWORD("ptr", identifier) >> dsl::p<ref_data>;
    static constexpr auto build = &lauf_build_ptr;
};
struct inst_local_addr
{
    static constexpr auto rule  = LEXY_KEYWORD("local_addr", identifier) >> dsl::p<ref_local_var>;
    static constexpr auto build = &lauf_build_local_addr;
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

struct inst_call
{
    static constexpr auto rule  = LEXY_KEYWORD("call", identifier) >> dsl::p<ref_function>;
    static constexpr auto build = &lauf_build_call;
};
struct inst_call_builtin
{
    static constexpr auto rule  = LEXY_KEYWORD("call_builtin", identifier) >> dsl::p<ref_builtin>;
    static constexpr auto build = &lauf_build_call_builtin;
};

struct inst_array_element
{
    static constexpr auto rule  = LEXY_KEYWORD("array_element", identifier) >> dsl::p<ref_type>;
    static constexpr auto build = &lauf_build_array_element;
};
struct inst_load_field
{
    static constexpr auto rule = LEXY_KEYWORD("load_field", identifier)
                                 >> dsl::p<ref_type> + dsl::period + dsl::integer<size_t>;
    static constexpr auto build = &lauf_build_load_field;
};
struct inst_store_field
{
    static constexpr auto rule = LEXY_KEYWORD("store_field", identifier)
                                 >> dsl::p<ref_type> + dsl::period + dsl::integer<size_t>;
    static constexpr auto build = &lauf_build_store_field;
};
struct inst_load_value
{
    static constexpr auto rule  = LEXY_KEYWORD("load_value", identifier) >> dsl::p<ref_local_var>;
    static constexpr auto build = &lauf_build_load_value;
};
struct inst_store_value
{
    static constexpr auto rule  = LEXY_KEYWORD("store_value", identifier) >> dsl::p<ref_local_var>;
    static constexpr auto build = &lauf_build_store_value;
};

struct inst_panic
{
    static constexpr auto rule  = LEXY_KEYWORD("panic", identifier);
    static constexpr auto build = &lauf_build_panic;
};
struct inst_panic_if
{
    static constexpr auto rule  = LEXY_KEYWORD("panic_if", identifier) >> dsl::symbol<ccs>;
    static constexpr auto build = &lauf_build_panic_if;
};

struct inst
{
    static constexpr auto rule
        = dsl::p<debug_location>
          + (dsl::p<inst_int> | dsl::p<inst_ptr> | dsl::p<inst_local_addr>                     //
             | dsl::p<inst_drop> | dsl::p<inst_pick> | dsl::p<inst_roll>                       //
             | dsl::p<inst_call> | dsl::p<inst_call_builtin>            //
             | dsl::p<inst_array_element> | dsl::p<inst_load_field> | dsl::p<inst_store_field> //
             | dsl::p<inst_load_value> | dsl::p<inst_store_value>                              //
             | dsl::p<inst_panic> | dsl::p<inst_panic_if>)                                     //
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
    static constexpr auto rule = [] {
        auto array_size = dsl::square_bracketed(dsl::integer<std::size_t>);
        auto type       = dsl::p<ref_type> + dsl::if_(array_size);

        return LEXY_KEYWORD("local", identifier)
               >> dsl::p<local_label> + dsl::colon + type + dsl::semicolon;
    }();
};

struct function
{
    struct header
    {
        static constexpr auto rule = LEXY_KEYWORD("function", identifier)
                                     >> dsl::position + dsl::p<global_label> + dsl::p<signature>;
    };

    static constexpr auto rule
        = dsl::p<header> >> (dsl::curly_bracketed.list(dsl::p<block> | dsl::p<local>)
                             | dsl::semicolon >> dsl::nullopt);
};

//=== module ===//
struct const_
{
    struct byte
    {
        static constexpr auto rule
            = LEXY_LIT("0x") >> dsl::integer<unsigned char>(dsl::digits<dsl::hex>)
              | dsl::integer<unsigned char>;
        static constexpr auto value = lexy::construct<char>;
    };

    struct string
    {
        static constexpr auto rule  = dsl::quoted(dsl::ascii::print);
        static constexpr auto value = lexy::as_string<std::string>;
    };

    struct data
    {
        static constexpr auto rule = dsl::list(dsl::p<byte> | dsl::p<string>, dsl::sep(dsl::comma));
        static constexpr auto value = lexy::as_string<std::string>;
    };

    static constexpr auto rule
        = LEXY_KEYWORD("const", identifier)
          >> dsl::p<global_label> + dsl::equal_sign + dsl::p<data> + dsl::semicolon;
};

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

    static constexpr auto rule
        = dsl::p<header> >> dsl::while_(dsl::p<function> | dsl::p<const_>) + dsl::eof;
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
    lauf_frontend_text_parser                parser;
    lexy::string_input<lexy::ascii_encoding> input;
    const char*                              path;

    mutable lauf_module_builder                          mod = nullptr;
    mutable lexy::input_location_anchor<decltype(input)> anchor;

    mutable lauf_function_builder                             cur_function = nullptr;
    mutable std::map<std::string_view, lauf_function_builder> function_labels;

    mutable std::size_t                                     cur_block = 0;
    mutable std::vector<partial_block>                      blocks;
    mutable std::map<std::string_view, lauf_block_builder>  block_labels;
    mutable std::map<std::string_view, lauf_local_variable> var_labels;
    mutable std::map<std::string_view, lauf_value_ptr>      data_labels;

    builder(lauf_frontend_text_parser parser, lexy::string_input<lexy::ascii_encoding> input,
            const char* path)
    : parser(parser), input(input), path(path), anchor(input)
    {}

    auto value_of(lauf::text_grammar::module_::header) const
    {
        return lexy::callback([&](std::string_view name) {
            mod = lauf_build_module(parser->intern(name));
            lauf_build_module_path(mod, parser->intern(path));
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

    auto value_of(lauf::text_grammar::const_) const
    {
        return lexy::callback([&](std::string_view name, std::string&& data) {
            data_labels[name] = parser->intern(std::move(data));
        });
    }

    auto value_of(lauf::text_grammar::function::header) const
    {
        return lexy::callback([&](const char* position, std::string_view name, lauf_signature sig) {
            auto iter = function_labels.find(name);
            if (iter != function_labels.end())
            {
                cur_function = iter->second;
            }
            else
            {
                cur_function          = lauf_build_function(mod, parser->intern(name), sig);
                function_labels[name] = cur_function;
            }

            // We keep the latest location in the file, which is presumably the definition.
            auto location = lexy::get_input_location(input, position, anchor);
            anchor        = location.anchor();
            lauf_build_function_debug_location(cur_function,
                                               lauf_debug_location{location.line_nr(),
                                                                   location.column_nr(), 0});
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
                       var_labels.clear();
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
        return lexy::callback(
            [&](std::string_view name, lauf_type type) {
                var_labels[name] = lauf_build_local_variable(cur_function, type->layout);
            },
            [&](std::string_view name, lauf_type type, std::size_t array_size) {
                auto layout = type->layout;
                layout.size *= array_size;
                var_labels[name] = lauf_build_local_variable(cur_function, layout);
            });
    }

    auto value_of(lauf::text_grammar::ref_local_var) const
    {
        return lexy::callback<lauf_local_variable>(
            [&](std::string_view name) { return var_labels.at(name); });
    }
    auto value_of(lauf::text_grammar::ref_function) const
    {
        return lexy::callback<lauf_function_builder>(
            [&](std::string_view name) { return function_labels.at(name); });
    }
    auto value_of(lauf::text_grammar::ref_data) const
    {
        return lexy::callback<const void*>(
            [&](std::string_view name) { return data_labels.at(name); });
    }
    auto value_of(lauf::text_grammar::ref_builtin) const
    {
        return lexy::callback<lauf_builtin>(
            [&](std::string_view name) { return parser->builtins.at(name); });
    }
    auto value_of(lauf::text_grammar::ref_type) const
    {
        return lexy::callback<lauf_type>(
            [&](std::string_view name) { return parser->types.at(name); });
    }

    auto value_of(lauf::text_grammar::debug_location) const
    {
        return lexy::callback([&](const char* position) {
            auto location = lexy::get_input_location(input, position, anchor);
            anchor        = location.anchor();
            lauf_build_debug_location(blocks[cur_block].builder,
                                      lauf_debug_location{location.line_nr(), location.column_nr(),
                                                          0});
        });
    }

    template <typename Inst, typename = decltype(Inst::build)>
    auto value_of(Inst) const
    {
        return lexy::callback(
            [&](const auto&... args) { Inst::build(blocks[cur_block].builder, args...); });
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

lauf_module lauf_frontend_text(lauf_frontend_text_parser p, const char* path, const char* data,
                               size_t size)
{
    auto input  = lexy::string_input<lexy::ascii_encoding>(data, size);
    auto result = lexy::parse<lauf::text_grammar::module_>(input, builder(p, input, path),
                                                           lexy_ext::report_error);
    return result ? result.value() : nullptr;
}

lauf_module lauf_frontend_text_cstr(lauf_frontend_text_parser p, const char* str)
{
    return lauf_frontend_text(p, "<literal>", str, std::strlen(str));
}

void lauf_frontend_text_destroy_parser(lauf_frontend_text_parser p)
{
    for (auto mod : p->modules)
        lauf_module_destroy(mod);
    delete p;
}

