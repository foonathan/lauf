// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/frontend/text.h>

#include <cstring>
#include <lauf/builder.h>
#include <lauf/detail/verify.hpp>
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

namespace
{
template <typename T>
class symbol_table
{
public:
    const T& lookup(std::string_view name) const
    {
        auto iter = map.find(name);
        if (iter != map.end())
            return iter->second;

        lauf::_detail::verification_failure("text",
                                            ("unknown name '" + std::string(name) + "'").c_str());
        return iter->second;
    }

    void insert(std::string_view name, T&& data)
    {
        auto [pos, inserted] = map.emplace(name, std::move(data));
        LAUF_VERIFY(inserted, "text", ("duplicate name '" + std::string(name) + "'").c_str());
    }
    void insert(std::string_view name, const T& data)
    {
        auto [pos, inserted] = map.emplace(name, data);
        LAUF_VERIFY(inserted, "text", ("duplicate name '" + std::string(name) + "'").c_str());
    }

    auto begin() const
    {
        return map.begin();
    }
    auto end() const
    {
        return map.end();
    }

private:
    std::map<std::string_view, T> map;
};
} // namespace

struct lauf_frontend_text_parser_impl
{
    lauf_builder               b;
    std::vector<lauf_module>   modules;
    symbol_table<lauf_builtin> builtins;
    symbol_table<lauf_type>    types;
    std::set<std::string>      data;

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

lauf_frontend_text_parser lauf_frontend_text_create_parser(void)
{
    return new lauf_frontend_text_parser_impl{lauf_builder_create()};
}

void lauf_frontend_text_register_builtin(lauf_frontend_text_parser p, const char* name,
                                         lauf_builtin builtin)
{
    p->builtins.insert(name, builtin);
}

void lauf_frontend_text_register_type(lauf_frontend_text_parser p, const char* name, lauf_type type)
{
    p->types.insert(name, type);
}

//=== common elements ===//
namespace lauf::text_grammar
{
namespace dsl = lexy::dsl;

constexpr auto whitespace = dsl::ascii::space | LEXY_LIT("#") >> dsl::until(dsl::newline).or_eof();

struct debug_location
{
    static constexpr auto rule = dsl::position;
};

constexpr auto identifier
    = dsl::identifier(dsl::ascii::alpha_underscore, dsl::ascii::alpha_digit_underscore);

struct local_identifier
{
    static constexpr auto rule  = dsl::percent_sign >> identifier;
    static constexpr auto value = lexy::as_string<std::string_view>;
};
struct global_identifier
{
    static constexpr auto rule  = dsl::at_sign >> identifier;
    static constexpr auto value = lexy::as_string<std::string_view>;
};

struct ref_type
{
    static constexpr auto rule = dsl::p<global_identifier>;
};
struct ref_label
{
    static constexpr auto rule = dsl::p<local_identifier>;
};
struct ref_local
{
    static constexpr auto rule = dsl::p<local_identifier>;
};
struct ref_function
{
    static constexpr auto rule = dsl::p<global_identifier>;
};
struct ref_global
{
    static constexpr auto rule = dsl::p<global_identifier>;
};
struct ref_builtin
{
    static constexpr auto rule = dsl::p<global_identifier>;
};
} // namespace lauf::text_grammar

//=== top-level declarations ===//
namespace lauf::text_grammar
{
struct signature
{
    static constexpr auto rule
        = dsl::parenthesized(dsl::times<2>(dsl::integer<uint8_t>, dsl::sep(LEXY_LIT("=>"))));
    static constexpr auto value = lexy::construct<lauf_signature>;
};

struct data_expr
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

    struct repeat
    {
        static constexpr auto rule = dsl::square_bracketed(dsl::recurse<data_expr>)
                                     >> dsl::lit_c<'*'> + dsl::integer<std::size_t>;
        static constexpr auto value
            = lexy::callback<std::string>([](const std::string& inner, std::size_t repetition) {
                  auto result = inner;
                  for (auto i = 1u; i < repetition; ++i)
                      result += inner;
                  return result;
              });
    };

    static constexpr auto rule
        = dsl::list(dsl::p<byte> | dsl::p<string> | dsl::p<repeat>, dsl::sep(dsl::comma));
    static constexpr auto value = lexy::as_string<std::string>;
};

struct const_decl
{
    static constexpr auto rule
        = LEXY_KEYWORD("const", identifier)
          >> dsl::p<global_identifier> + dsl::equal_sign + dsl::p<data_expr> + dsl::semicolon;
};
struct data_decl
{
    static constexpr auto rule = [] {
        auto zero = LEXY_LIT("zero") >> dsl::lit_c<'*'> + dsl::integer<std::size_t>;
        auto expr = dsl::else_ >> dsl::p<data_expr>;

        return LEXY_KEYWORD("data", identifier)
               >> dsl::p<global_identifier> + dsl::equal_sign + (zero | expr) + dsl::semicolon;
    }();
};

struct function_decl
{
    struct body
    {
        static constexpr auto rule = [] {
            auto impl = dsl::lit_c<'{'> >> dsl::loop(dsl::lit_c<'}'> >> dsl::break_ | whitespace
                                                     | dsl::else_ >> dsl::code_point);
            return dsl::capture(dsl::token(impl));
        }();
        static constexpr auto value = lexy::as_string<std::string_view>;
    };

    static constexpr auto rule = [] {
        auto header = dsl::p<global_identifier> + dsl::p<signature>;

        return LEXY_KEYWORD("function", identifier) >> header + dsl::p<body>;
    }();
};

struct module_decl
{
    static constexpr auto whitespace = text_grammar::whitespace;

    struct header
    {
        static constexpr auto rule = dsl::p<global_identifier> + dsl::semicolon;
    };

    static constexpr auto rule = [] {
        auto decls
            = dsl::list(dsl::p<function_decl> | dsl::p<const_decl> | dsl::p<data_decl>) + dsl::eof;

        return LEXY_KEYWORD("module", identifier) >> dsl::p<header> + decls;
    }();
};
} // namespace lauf::text_grammar

namespace
{
struct function_decl
{
    lauf_function_decl decl;
    std::string_view   body;
};

struct module_decl
{
    symbol_table<lauf_global>   global;
    symbol_table<function_decl> functions;
};

auto parse_module_decls(lauf_frontend_text_parser p, const char* path,
                        lexy::string_input<lexy::ascii_encoding> input)
{
    struct parser
    {
        lauf_frontend_text_parser                p;
        lexy::string_input<lexy::ascii_encoding> input;
        const char*                              path;
        mutable module_decl                      result;

        parser(lauf_frontend_text_parser p, lexy::string_input<lexy::ascii_encoding> input,
               const char* path)
        : p(p), input(input), path(path)
        {}

        auto value_of(lauf::text_grammar::module_decl::header) const
        {
            return lexy::callback(
                [&](std::string_view name) { lauf_build_module(p->b, p->intern(name), path); });
        }

        auto value_of(lauf::text_grammar::const_decl) const
        {
            return lexy::callback([&](std::string_view name, std::string&& data) {
                auto size   = data.size();
                auto memory = p->intern(std::move(data));
                result.global.insert(name, lauf_build_const(p->b, memory, size));
            });
        }
        auto value_of(lauf::text_grammar::data_decl) const
        {
            return lexy::callback(
                [&](std::string_view name, std::string&& data) {
                    auto size   = data.size();
                    auto memory = p->intern(std::move(data));
                    result.global.insert(name, lauf_build_data(p->b, memory, size));
                },
                [&](std::string_view name, std::size_t size) {
                    result.global.insert(name, lauf_build_zero_data(p->b, size));
                });
        }

        auto value_of(lauf::text_grammar::function_decl) const
        {
            return lexy::callback(
                [&](std::string_view name, lauf_signature sig, std::string_view body) {
                    auto decl = lauf_declare_function(p->b, p->intern(name), sig);
                    result.functions.insert(name, {decl, body});
                });
        }

        auto value_of(lauf::text_grammar::module_decl) const
        {
            return lexy::noop >> lexy::callback<module_decl>([&] { return std::move(result); });
        }
    };

    return lexy::parse<lauf::text_grammar::module_decl>(input, parser(p, input, path),
                                                        lexy_ext::report_error);
}
} // namespace

//=== function body declarations ===//
namespace
{
struct function_body_decls
{
    symbol_table<lauf_label> labels;
    symbol_table<lauf_local> locals;
};
} // namespace

namespace lauf::text_grammar
{
struct skip_instruction
{
    static constexpr auto rule  = identifier >> dsl::loop(dsl::semicolon >> dsl::break_ | whitespace
                                                          | dsl::else_ >> dsl::code_point);
    static constexpr auto value = lexy::noop;
};

struct label_decl
{
    static constexpr auto rule
        = LEXY_KEYWORD("label", identifier)
          >> dsl::p<local_identifier> + dsl::opt(dsl::parenthesized(dsl::integer<std::uint8_t>))
                 + dsl::colon;
};

struct layout
{
    static constexpr auto rule
        = dsl::parenthesized(dsl::twice(dsl::integer<std::size_t>, dsl::sep(dsl::comma)));
    static constexpr auto value = lexy::construct<lauf_layout>;
};

struct local_decl
{
    static constexpr auto rule = [] {
        auto array_size = dsl::square_bracketed(dsl::integer<std::size_t>);
        auto type       = (dsl::p<ref_type> | dsl::p<layout>)+dsl::opt(array_size);

        return LEXY_KEYWORD("local", identifier)
               >> dsl::p<local_identifier> + dsl::colon + type + dsl::semicolon;
    }();
};

struct function_body_decls
{
    static constexpr auto whitespace = text_grammar::whitespace;
    static constexpr auto rule       = dsl::curly_bracketed.opt_list(
              dsl::p<label_decl> | dsl::p<local_decl> | dsl::p<skip_instruction>);
};
} // namespace lauf::text_grammar

namespace
{
auto parse_function_body_decls(lauf_frontend_text_parser p,
                               lexy::string_input<lexy::ascii_encoding>, std::string_view body)
{
    struct parser
    {
        lauf_frontend_text_parser   p;
        mutable function_body_decls result;

        auto value_of(lauf::text_grammar::ref_type) const
        {
            return lexy::callback<lauf_type>(
                [&](std::string_view name) { return p->types.lookup(name); });
        }

        auto value_of(lauf::text_grammar::label_decl) const
        {
            return lexy::callback(
                [&](std::string_view name, std::optional<std::uint8_t> stack_size) {
                    result.labels.insert(name, lauf_declare_label(p->b, stack_size.value_or(0)));
                });
        }

        auto value_of(lauf::text_grammar::local_decl) const
        {
            auto make_local = [&](std::string_view name, lauf_layout layout,
                                  std::optional<std::size_t> array_size) {
                layout.size *= array_size.value_or(1);
                result.locals.insert(name, lauf_build_local_variable(p->b, layout));
            };

            return lexy::callback(make_local, [make_local](std::string_view name, lauf_type type,
                                                           std::optional<std::size_t> array_size) {
                auto layout = type->layout;
                make_local(name, layout, array_size);
            });
        }

        auto value_of(lauf::text_grammar::function_body_decls) const
        {
            return lexy::noop >> lexy::callback<function_body_decls>(
                       [&](const auto&...) { return std::move(result); });
        }
    };

    auto body_input = lexy::string_input<lexy::ascii_encoding>(body);
    return lexy::parse<lauf::text_grammar::function_body_decls>(body_input, parser{p, {}},
                                                                // TODO: support partial input for
                                                                // correct location
                                                                lexy_ext::report_error);
}
} // namespace

//=== function body ===//
namespace lauf::text_grammar
{
static constexpr auto ccs = lexy::symbol_table<lauf_condition> //
                                .map<LEXY_SYMBOL("is_true")>(LAUF_IS_TRUE)
                                .map<LEXY_SYMBOL("is_false")>(LAUF_IS_FALSE)
                                .map<LEXY_SYMBOL("cmp_eq")>(LAUF_CMP_EQ)
                                .map<LEXY_SYMBOL("cmp_ne")>(LAUF_CMP_NE)
                                .map<LEXY_SYMBOL("cmp_lt")>(LAUF_CMP_LT)
                                .map<LEXY_SYMBOL("cmp_le")>(LAUF_CMP_LE)
                                .map<LEXY_SYMBOL("cmp_gt")>(LAUF_CMP_GT)
                                .map<LEXY_SYMBOL("cmp_ge")>(LAUF_CMP_GE);

struct inst_return
{
    static constexpr auto rule  = LEXY_KEYWORD("return", identifier);
    static constexpr auto build = &lauf_build_return;
};
struct inst_jump
{
    static constexpr auto rule  = LEXY_KEYWORD("jump", identifier) >> dsl::p<ref_label>;
    static constexpr auto build = &lauf_build_jump;
};
struct inst_jump_if
{
    static constexpr auto rule
        = LEXY_KEYWORD("jump_if", identifier) >> dsl::symbol<ccs> + dsl::p<ref_label>;
    static constexpr auto build = &lauf_build_jump_if;
};

struct inst_int
{
    static constexpr auto rule
        = LEXY_KEYWORD("int", identifier)
          >> (LEXY_LIT("0x") >> dsl::integer<lauf_value_uint>(dsl::digits<dsl::hex>)
              | dsl::integer<lauf_value_uint>);
    static constexpr auto build = &lauf_build_int;
};
struct inst_global_addr
{
    static constexpr auto rule  = LEXY_KEYWORD("global_addr", identifier) >> dsl::p<ref_global>;
    static constexpr auto build = &lauf_build_global_addr;
};
struct inst_local_addr
{
    static constexpr auto rule  = LEXY_KEYWORD("local_addr", identifier) >> dsl::p<ref_local>;
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
struct inst_select
{
    static constexpr auto rule  = LEXY_KEYWORD("select", identifier) >> dsl::integer<size_t>;
    static constexpr auto build = &lauf_build_select;
};
struct inst_select_if
{
    static constexpr auto rule  = LEXY_KEYWORD("select_if", identifier) >> dsl::symbol<ccs>;
    static constexpr auto build = &lauf_build_select_if;
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

struct inst_array_element_addr
{
    static constexpr auto rule = LEXY_KEYWORD("array_element_addr", identifier) >> dsl::p<ref_type>;
    static constexpr auto build = &lauf_build_array_element_addr;
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
    static constexpr auto rule  = LEXY_KEYWORD("load_value", identifier) >> dsl::p<ref_local>;
    static constexpr auto build = &lauf_build_load_value;
};
struct inst_load_array_value
{
    static constexpr auto rule  = LEXY_KEYWORD("load_array_value", identifier) >> dsl::p<ref_local>;
    static constexpr auto build = &lauf_build_load_array_value;
};
struct inst_store_value
{
    static constexpr auto rule  = LEXY_KEYWORD("store_value", identifier) >> dsl::p<ref_local>;
    static constexpr auto build = &lauf_build_store_value;
};
struct inst_store_array_value
{
    static constexpr auto rule = LEXY_KEYWORD("store_array_value", identifier) >> dsl::p<ref_local>;
    static constexpr auto build = &lauf_build_store_array_value;
};

struct inst_poison_alloc
{
    static constexpr auto rule  = LEXY_KEYWORD("poison_alloc", identifier);
    static constexpr auto build = &lauf_build_poison_alloc;
};
struct inst_unpoison_alloc
{
    static constexpr auto rule  = LEXY_KEYWORD("unpoison_alloc", identifier);
    static constexpr auto build = &lauf_build_unpoison_alloc;
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
    static constexpr auto rule = [] {
        auto insts = dsl::p<inst_return> | dsl::p<inst_jump> | dsl::p<inst_jump_if>          //
                     | dsl::p<inst_int> | dsl::p<inst_global_addr> | dsl::p<inst_local_addr> //
                     | dsl::p<inst_drop> | dsl::p<inst_pick> | dsl::p<inst_roll>             //
                     | dsl::p<inst_select> | dsl::p<inst_select_if>                          //
                     | dsl::p<inst_call> | dsl::p<inst_call_builtin>                         //
                     | dsl::p<inst_array_element_addr>                                       //
                     | dsl::p<inst_load_field> | dsl::p<inst_store_field>                    //
                     | dsl::p<inst_load_value> | dsl::p<inst_load_array_value>               //
                     | dsl::p<inst_store_value> | dsl::p<inst_store_array_value>             //
                     | dsl::p<inst_poison_alloc> | dsl::p<inst_unpoison_alloc>               //
                     | dsl::p<inst_panic> | dsl::p<inst_panic_if>;
        return dsl::p<debug_location> + insts + dsl::semicolon;
    }();
    static constexpr auto value = lexy::forward<void>;
};

struct function_body
{
    static constexpr auto whitespace = text_grammar::whitespace;
    static constexpr auto rule       = dsl::curly_bracketed.opt_list(
              dsl::p<local_decl> | dsl::p<label_decl> | dsl::else_ >> dsl::p<inst>);

    static constexpr auto value = lexy::noop >> lexy::callback<int>([](const auto&...) {
                                      return 0;
                                  }); // TODO: void not allowed by lexy
};
} // namespace lauf::text_grammar

namespace
{
struct function_body_parser
{
    lauf_frontend_text_parser                            p;
    lexy::string_input<lexy::ascii_encoding>             input;
    const module_decl*                                   mod;
    const function_body_decls*                           body;
    mutable lexy::input_location_anchor<decltype(input)> anchor;

    function_body_parser(lauf_frontend_text_parser                p,
                         lexy::string_input<lexy::ascii_encoding> input, const module_decl& mod,
                         const function_body_decls& body)
    : p(p), input(input), mod(&mod), body(&body), anchor(input)
    {}

    auto value_of(lauf::text_grammar::debug_location) const
    {
        return lexy::callback([&](const char* pos) {
            auto location = lexy::get_input_location(input, pos, anchor);
            anchor        = location.anchor();
            lauf_build_debug_location(p->b, lauf_debug_location{location.line_nr(),
                                                                location.column_nr(), 0});
        });
    }

    auto value_of(lauf::text_grammar::ref_local) const
    {
        return lexy::callback<lauf_local>(
            [&](std::string_view name) { return body->locals.lookup(name); });
    }
    auto value_of(lauf::text_grammar::ref_label) const
    {
        return lexy::callback<lauf_label>(
            [&](std::string_view name) { return body->labels.lookup(name); });
    }
    auto value_of(lauf::text_grammar::ref_function) const
    {
        return lexy::callback<lauf_function_decl>(
            [&](std::string_view name) { return mod->functions.lookup(name).decl; });
    }
    auto value_of(lauf::text_grammar::ref_global) const
    {
        return lexy::callback<lauf_global>(
            [&](std::string_view name) { return mod->global.lookup(name); });
    }
    auto value_of(lauf::text_grammar::ref_builtin) const
    {
        return lexy::callback<lauf_builtin>(
            [&](std::string_view name) { return p->builtins.lookup(name); });
    }
    auto value_of(lauf::text_grammar::ref_type) const
    {
        return lexy::callback<lauf_type>(
            [&](std::string_view name) { return p->types.lookup(name); });
    }

    auto value_of(lauf::text_grammar::local_decl) const
    {
        // No need to do anything on the second pass.
        return lexy::noop;
    }
    auto value_of(lauf::text_grammar::label_decl) const
    {
        return lexy::callback([&](std::string_view name, std::optional<std::uint8_t>) {
            lauf_place_label(p->b, body->labels.lookup(name));
        });
    }
    template <typename Inst, typename = decltype(Inst::build)>
    auto value_of(Inst) const
    {
        return lexy::callback([&](const auto&... args) { Inst::build(p->b, args...); });
    }
};

auto parse_function_body(lauf_frontend_text_parser p, const module_decl& mod,
                         const function_body_decls&               body_decls,
                         lexy::string_input<lexy::ascii_encoding> input, std::string_view body)
{
    auto body_input = lexy::string_input<lexy::ascii_encoding>(body);

    return lexy::parse<lauf::text_grammar::function_body>( //
        body_input, function_body_parser(p, input, mod, body_decls),
        // TODO: support partial input for correct location
        lexy_ext::report_error);
}
} // namespace

lauf_module lauf_frontend_text(lauf_frontend_text_parser p, const char* path, const char* data,
                               size_t size)
{
    auto input    = lexy::string_input<lexy::ascii_encoding>(data, size);
    auto mod_decl = parse_module_decls(p, path, input);
    if (!mod_decl)
        return nullptr;

    for (auto& [name, function] : mod_decl.value().functions)
    {
        lauf_build_function(p->b, function.decl);

        auto body_decls = parse_function_body_decls(p, input, function.body);
        if (!body_decls)
            return nullptr;

        if (!parse_function_body(p, mod_decl.value(), body_decls.value(), input, function.body))
            return nullptr;

        lauf_finish_function(p->b);
    }

    return lauf_finish_module(p->b);
}

lauf_module lauf_frontend_text_cstr(lauf_frontend_text_parser p, const char* str)
{
    return lauf_frontend_text(p, "<literal>", str, std::strlen(str));
}

void lauf_frontend_text_destroy_parser(lauf_frontend_text_parser p)
{
    for (auto mod : p->modules)
        lauf_module_destroy(mod);
    lauf_builder_destroy(p->b);
    delete p;
}

