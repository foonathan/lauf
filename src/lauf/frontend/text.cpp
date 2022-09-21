// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/frontend/text.h>

#include <lauf/asm/builder.h>
#include <lauf/asm/module.h>
#include <lauf/asm/type.h>
#include <lauf/reader.hpp>
#include <lauf/runtime/builtin.h>
#include <lexy/action/parse.hpp>
#include <lexy/callback.hpp>
#include <lexy/dsl.hpp>
#include <lexy/input_location.hpp>
#include <lexy_ext/report_error.hpp>
#include <map>
#include <optional>
#include <string>
#include <vector>

extern "C"
{
    extern const lauf_runtime_builtin_library* lauf_libs;
    extern const size_t                        lauf_libs_count;
}

const lauf_frontend_text_options lauf_frontend_default_text_options = {lauf_libs, lauf_libs_count};

namespace
{
template <typename T>
class symbol_table
{
public:
    const T* try_lookup(std::string_view name) const
    {
        auto iter = map.find(name);
        if (iter != map.end())
            return &iter->second;
        else
            return nullptr;
    }

    void clear()
    {
        map.clear();
    }

    bool insert(std::string_view name, const T& data)
    {
        auto [pos, inserted] = map.emplace(name, data);
        return inserted;
    }

private:
    std::map<std::string, T, std::less<>> map;
};

struct parse_state
{
    lauf_reader*                                       input;
    lauf_asm_builder*                                  builder;
    symbol_table<const lauf_runtime_builtin_function*> builtins;
    symbol_table<const lauf_asm_type*>                 types;

    lauf_asm_module*                 mod;
    lauf_asm_function*               fn;
    symbol_table<lauf_asm_global*>   globals;
    symbol_table<lauf_asm_function*> functions;
    symbol_table<lauf_asm_block*>    blocks;
    symbol_table<lauf_asm_local*>    locals;

    lexy::input_location_anchor<lexy::buffer<lexy::utf8_encoding>> anchor;

    explicit parse_state(lauf_reader* input, lauf_frontend_text_options opts)
    : input(input), builder(lauf_asm_create_builder(lauf_asm_default_build_options)), mod(nullptr),
      anchor(input->buffer)
    {
        types.insert(lauf_asm_type_value.name, &lauf_asm_type_value);

        for (auto i = 0u; i != opts.builtin_libs_count; ++i)
        {
            for (auto builtin = opts.builtin_libs[i].functions; builtin != nullptr;
                 builtin      = builtin->next)
                builtins.insert(opts.builtin_libs[i].prefix + std::string(".") + builtin->name,
                                     builtin);

            for (auto type = opts.builtin_libs[i].types; type != nullptr; type = type->next)
                types.insert(opts.builtin_libs[i].prefix + std::string(".") + type->name, type);
        }
    }

    auto report_error() const
    {
        return lexy_ext::report_error.opts({lexy::visualize_fancy}).path(input->path);
    }

    void unknown_identifier(const LEXY_CHAR8_T* position, const char* kind, const char* name) const
    {
        auto loc = lexy::get_input_location(input->buffer, position, anchor);

        auto out = lexy::cfile_output_iterator{stderr};
        lexy_ext::diagnostic_writer<decltype(input->buffer)> writer(input->buffer,
                                                                    {lexy::visualize_fancy});

        writer.write_message(out, lexy_ext::diagnostic_kind::error,
                             [&](auto, lexy::visualization_options) {
                                 std::fprintf(stderr, "unknown %s name '%s'", kind, name);
                                 return out;
                             });
        if (input->path != nullptr)
            writer.write_path(out, input->path);
        writer.write_empty_annotation(out);
        writer.write_annotation(out, lexy_ext::annotation_kind::primary, loc, std::strlen(name) + 1,
                                [&](auto, lexy::visualization_options) {
                                    std::fprintf(stderr, "used here");
                                    return out;
                                });
    }
    void duplicate_declaration(const LEXY_CHAR8_T* position, const char* kind,
                               const char* name) const
    {
        auto loc = lexy::get_input_location(input->buffer, position, anchor);

        auto out = lexy::cfile_output_iterator{stderr};
        lexy_ext::diagnostic_writer<decltype(input->buffer)> writer(input->buffer,
                                                                    {lexy::visualize_fancy});

        writer.write_message(out, lexy_ext::diagnostic_kind::error,
                             [&](auto, lexy::visualization_options) {
                                 std::fprintf(stderr, "duplicate %s declaration named '%s'", kind,
                                              name);
                                 return out;
                             });
        if (input->path != nullptr)
            writer.write_path(out, input->path);
        writer.write_empty_annotation(out);
        writer.write_annotation(out, lexy_ext::annotation_kind::primary, loc, std::strlen(name) + 1,
                                [&](auto, lexy::visualization_options) {
                                    std::fprintf(stderr, "second declaration here");
                                    return out;
                                });
    }
    void conflicting_signature(const LEXY_CHAR8_T* position, const char* kind,
                               const char* name) const
    {
        auto loc = lexy::get_input_location(input->buffer, position, anchor);

        auto out = lexy::cfile_output_iterator{stderr};
        lexy_ext::diagnostic_writer<decltype(input->buffer)> writer(input->buffer,
                                                                    {lexy::visualize_fancy});

        writer.write_message(out, lexy_ext::diagnostic_kind::error,
                             [&](auto, lexy::visualization_options) {
                                 std::fprintf(stderr,
                                              "conflicting signature in %s declaration named '%s'",
                                              kind, name);
                                 return out;
                             });
        if (input->path != nullptr)
            writer.write_path(out, input->path);
        writer.write_empty_annotation(out);
        writer.write_annotation(out, lexy_ext::annotation_kind::primary, loc, std::strlen(name) + 1,
                                [&](auto, lexy::visualization_options) {
                                    std::fprintf(stderr, "second declaration here");
                                    return out;
                                });
    }
};
} // namespace

namespace lauf::text_grammar
{
namespace dsl = lexy::dsl;

template <typename Ret = void, typename... Fn>
constexpr auto callback(Fn... fn)
{
    return lexy::bind(lexy::callback<Ret>(fn...), lexy::parse_state, lexy::values);
}

//=== common ===//
struct identifier
{
    static constexpr auto unquoted
        = dsl::identifier(dsl::ascii::alpha_underscore / dsl::period,
                          dsl::ascii::alpha_digit_underscore / dsl::period);

    static constexpr auto rule = [] {
        auto quoted = dsl::single_quoted(dsl::unicode::print);

        return unquoted | quoted;
    }();

    static constexpr auto value = lexy::as_string<std::string>;
};

#define LAUF_KEYWORD(Str) LEXY_KEYWORD(Str, identifier::unquoted)

struct builtin_identifier
{
    static constexpr auto rule  = dsl::dollar_sign >> dsl::p<identifier>;
    static constexpr auto value = lexy::forward<std::string>;
};
struct global_identifier
{
    static constexpr auto rule  = dsl::at_sign >> dsl::p<identifier>;
    static constexpr auto value = lexy::forward<std::string>;
};
struct local_identifier
{
    static constexpr auto rule  = dsl::percent_sign >> dsl::p<identifier>;
    static constexpr auto value = lexy::forward<std::string>;
};

struct signature
{
    static constexpr auto rule = [] {
        auto spec = dsl::integer<std::uint8_t> >> LEXY_LIT("=>") + dsl::integer<std::uint8_t>;
        return dsl::parenthesized.opt(spec);
    }();

    static constexpr auto value
        = lexy::callback<lauf_asm_signature>(lexy::construct<lauf_asm_signature>,
                                             [](lexy::nullopt) {
                                                 return lauf_asm_signature{0, 0};
                                             });
};

struct builtin_ref
{
    static constexpr auto rule  = dsl::position(dsl::p<builtin_identifier>);
    static constexpr auto value = callback<lauf_runtime_builtin_function>(
        [](const parse_state& state, auto pos, const std::string& name) {
            auto result = state.builtins.try_lookup(name);
            if (result == nullptr)
                state.unknown_identifier(pos, "builtin", name.c_str());
            return **result;
        });
};
struct type_ref
{
    static constexpr auto rule = dsl::position(dsl::p<builtin_identifier>);
    static constexpr auto value
        = callback<lauf_asm_type>([](const parse_state& state, auto pos, const std::string& name) {
              auto result = state.types.try_lookup(name);
              if (result == nullptr)
                  state.unknown_identifier(pos, "type", name.c_str());
              return **result;
          });
};
struct global_ref
{
    static constexpr auto rule  = dsl::position(dsl::p<global_identifier>);
    static constexpr auto value = callback<lauf_asm_global*>(
        [](const parse_state& state, auto pos, const std::string& name) {
            auto result = state.globals.try_lookup(name);
            if (result == nullptr)
                state.unknown_identifier(pos, "global", name.c_str());
            return *result;
        });
};
struct local_ref
{
    static constexpr auto rule  = dsl::position(dsl::p<local_identifier>);
    static constexpr auto value = callback<lauf_asm_local*>(
        [](const parse_state& state, auto pos, const std::string& name) {
            auto result = state.locals.try_lookup(name);
            if (result == nullptr)
                state.unknown_identifier(pos, "local", name.c_str());
            return *result;
        });
};
struct function_ref
{
    static constexpr auto rule
        = dsl::position + dsl::p<global_identifier> + dsl::if_(dsl::p<signature>);
    static constexpr auto value = callback<lauf_asm_function*>( //
        [](const parse_state& state, auto pos, const std::string& name) {
            auto result = state.functions.try_lookup(name);
            if (result == nullptr)
                state.unknown_identifier(pos, "function", name.c_str());
            return *result;
        },
        [](parse_state& state, auto pos, const std::string& name, lauf_asm_signature sig) {
            if (auto f = state.functions.try_lookup(name))
            {
                auto f_sig = lauf_asm_function_signature(*f);
                if (f_sig.input_count != sig.input_count || f_sig.output_count != sig.output_count)
                    state.conflicting_signature(pos, "function", name.c_str());
                return *f;
            }

            auto f = lauf_asm_add_function(state.mod, name.c_str(), sig);
            state.functions.insert(name, f);
            return f;
        });
};
struct block_ref
{
    static constexpr auto rule
        = dsl::position + dsl::p<local_identifier> + dsl::if_(dsl::p<signature>);
    static constexpr auto value = callback<lauf_asm_block*>( //
        [](const parse_state& state, auto pos, const std::string& name) {
            auto result = state.blocks.try_lookup(name);
            if (result == nullptr)
                state.unknown_identifier(pos, "block", name.c_str());
            return *result;
        },
        [](parse_state& state, auto, const std::string& name, lauf_asm_signature sig) {
            if (auto block = state.blocks.try_lookup(name))
                return *block;

            auto block = lauf_asm_declare_block(state.builder, sig.input_count);
            state.blocks.insert(name, block);
            return block;
        });
};

//=== layout ===//
struct layout_expr
{
    struct literal
    {
        static constexpr auto rule
            = dsl::parenthesized(dsl::twice(dsl::integer<std::size_t>, dsl::sep(dsl::comma)));
        static constexpr auto value = lexy::construct<lauf_asm_layout>;
    };

    struct type
    {
        static constexpr auto rule  = dsl::p<type_ref>;
        static constexpr auto value = lexy::mem_fn(&lauf_asm_type::layout);
    };

    struct array
    {
        static constexpr auto rule
            = dsl::square_bracketed(dsl::integer<std::size_t>) >> dsl::recurse<layout_expr>;
        static constexpr auto value
            = lexy::callback<lauf_asm_layout>([](std::size_t size, lauf_asm_layout layout) {
                  return lauf_asm_array_layout(layout, size);
              });
    };

    struct aggregate
    {
        static constexpr auto rule
            = dsl::curly_bracketed.opt_list(dsl::recurse<layout_expr>, dsl::sep(dsl::comma));
        static constexpr auto value
            = lexy::as_list<std::vector<lauf_asm_layout>> >> lexy::callback<lauf_asm_layout>(
                  [](const std::optional<std::vector<lauf_asm_layout>>& members) {
                      if (!members)
                          return lauf_asm_aggregate_layout(nullptr, 0);
                      else
                          return lauf_asm_aggregate_layout(members->data(), members->size());
                  });
    };

    static constexpr auto rule = dsl::p<literal> | dsl::p<type> | dsl::p<array> | dsl::p<aggregate>;
    static constexpr auto value = lexy::forward<lauf_asm_layout>;
};

//=== global ===//
struct data_expr
{
    struct byte
    {
        static constexpr auto rule = dsl::integer<unsigned char>(dsl::digits<dsl::hex>);
        static constexpr auto value
            = lexy::callback<std::string>([](unsigned char c) { return std::string(1, char(c)); });
    };

    struct string
    {
        static constexpr auto rule  = dsl::quoted(dsl::ascii::print);
        static constexpr auto value = lexy::as_string<std::string>;
    };

    struct repetition
    {
        static constexpr auto rule = dsl::square_bracketed(dsl::recurse<data_expr>)
                                     >> dsl::lit_c<'*'> + dsl::integer<std::size_t>;

        static constexpr auto value
            = lexy::callback<std::string>([](const std::string& data, std::size_t n) {
                  std::string result;
                  for (auto i = 0u; i != n; ++i)
                      result += data;
                  return result;
              });
    };

    static constexpr auto rule
        = dsl::list(dsl::p<byte> | dsl::p<string> | dsl::p<repetition>, dsl::sep(dsl::comma));
    static constexpr auto value = lexy::concat<std::string>;
};

struct global_decl
{
    struct const_global
    {
        static constexpr auto rule = [] {
            auto decl = dsl::p<global_identifier> + dsl::opt(dsl::colon >> dsl::p<layout_expr>)
                        + dsl::equal_sign + dsl::p<data_expr>;
            return LAUF_KEYWORD("const") >> dsl::position + decl;
        }();

        static constexpr auto value = callback(
            [](parse_state& state, auto pos, const std::string& name, lauf_asm_layout layout,
               std::string data) {
                data.resize(layout.size);
                auto g = lauf_asm_add_global_const_data(state.mod, data.c_str(), layout);
                lauf_asm_set_global_debug_name(state.mod, g, name.c_str());
                if (!state.globals.insert(name, g))
                    state.duplicate_declaration(pos, "global", name.c_str());
            },
            [](parse_state& state, auto pos, const std::string& name, lexy::nullopt,
               const std::string& data) {
                auto g = lauf_asm_add_global_const_data(state.mod, data.c_str(),
                                                        {data.size(), alignof(void*)});
                lauf_asm_set_global_debug_name(state.mod, g, name.c_str());
                if (!state.globals.insert(name, g))
                    state.duplicate_declaration(pos, "global", name.c_str());
            });
    };

    struct mut_global
    {
        static constexpr auto rule = [] {
            auto decl = dsl::p<global_identifier> + dsl::opt(dsl::colon >> dsl::p<layout_expr>)
                        + dsl::equal_sign + dsl::p<data_expr>;
            return dsl::else_ >> dsl::position + decl;
        }();

        static constexpr auto value = callback(
            [](parse_state& state, auto pos, const std::string& name, lauf_asm_layout layout,
               std::string data) {
                data.resize(layout.size);
                auto g = lauf_asm_add_global_mut_data(state.mod, data.c_str(), layout);
                lauf_asm_set_global_debug_name(state.mod, g, name.c_str());
                if (!state.globals.insert(name, g))
                    state.duplicate_declaration(pos, "global", name.c_str());
            },
            [](parse_state& state, auto pos, const std::string& name, lexy::nullopt,
               const std::string& data) {
                auto g = lauf_asm_add_global_mut_data(state.mod, data.c_str(),
                                                      {data.size(), alignof(void*)});
                lauf_asm_set_global_debug_name(state.mod, g, name.c_str());
                if (!state.globals.insert(name, g))
                    state.duplicate_declaration(pos, "global", name.c_str());
            });
    };

    static constexpr auto rule
        = LAUF_KEYWORD("global") >> (dsl::p<const_global> | dsl::p<mut_global>)+dsl::semicolon;
    static constexpr auto value = lexy::forward<void>;
};

//=== instruction ===//
template <typename Fn>
constexpr auto inst(Fn fn)
{
    return callback([fn](const parse_state& state, auto... args) { fn(state.builder, args...); });
}
constexpr auto inst()
{
    return callback([](const parse_state& state, auto fn, auto... args) //
                    { fn(state.builder, args...); });
}

struct inst_return
{
    static constexpr auto rule  = LAUF_KEYWORD("return");
    static constexpr auto value = inst(&lauf_asm_inst_return);
};
struct inst_panic
{
    static constexpr auto rule  = LAUF_KEYWORD("panic");
    static constexpr auto value = inst(&lauf_asm_inst_panic);
};
struct inst_jump
{
    static constexpr auto rule  = LAUF_KEYWORD("jump") >> dsl::p<block_ref>;
    static constexpr auto value = inst(&lauf_asm_inst_jump);
};
struct inst_branch
{
    static constexpr auto rule  = LAUF_KEYWORD("branch") >> dsl::p<block_ref> + dsl::p<block_ref>;
    static constexpr auto value = inst(&lauf_asm_inst_branch);
};

struct inst_sint
{
    struct integer : lexy::token_production
    {
        static constexpr auto rule = [] {
            auto dec = dsl::integer<lauf_sint>(dsl::digits<>.sep(dsl::lit_c<'_'>));
            auto hex = LEXY_LIT("0x")
                       >> dsl::integer<lauf_sint>(dsl::digits<dsl::hex_upper>.sep(dsl::lit_c<'_'>));
            return dsl::sign + (hex | dec);
        }();
        static constexpr auto value = lexy::as_integer<lauf_sint>;
    };

    static constexpr auto rule  = LAUF_KEYWORD("sint") >> dsl::p<integer>;
    static constexpr auto value = inst(&lauf_asm_inst_sint);
};
struct inst_uint
{
    struct integer : lexy::token_production
    {
        static constexpr auto rule = [] {
            auto dec = dsl::integer<lauf_uint>(dsl::digits<>.sep(dsl::lit_c<'_'>));
            auto hex = LEXY_LIT("0x")
                       >> dsl::integer<lauf_uint>(dsl::digits<dsl::hex_upper>.sep(dsl::lit_c<'_'>));
            return hex | dec;
        }();
        static constexpr auto value = lexy::as_integer<lauf_uint>;
    };

    static constexpr auto rule  = LAUF_KEYWORD("uint") >> dsl::p<integer>;
    static constexpr auto value = inst(&lauf_asm_inst_uint);
};
struct inst_null
{
    static constexpr auto rule  = LAUF_KEYWORD("null");
    static constexpr auto value = inst(&lauf_asm_inst_null);
};
struct inst_global_addr
{
    static constexpr auto rule  = LAUF_KEYWORD("global_addr") >> dsl::p<global_ref>;
    static constexpr auto value = inst(&lauf_asm_inst_global_addr);
};
struct inst_local_addr
{
    static constexpr auto rule  = LAUF_KEYWORD("local_addr") >> dsl::p<local_ref>;
    static constexpr auto value = inst(&lauf_asm_inst_local_addr);
};
struct inst_function_addr
{
    static constexpr auto rule  = LAUF_KEYWORD("function_addr") >> dsl::p<function_ref>;
    static constexpr auto value = inst(&lauf_asm_inst_function_addr);
};
struct inst_layout
{
    static constexpr auto rule  = LAUF_KEYWORD("layout") >> dsl::p<layout_expr>;
    static constexpr auto value = inst(&lauf_asm_inst_layout);
};
struct inst_cc
{
    static constexpr auto ccs = lexy::symbol_table<lauf_asm_inst_condition_code> //
                                    .map(LEXY_LIT("eq"), LAUF_ASM_INST_CC_EQ)
                                    .map(LEXY_LIT("ne"), LAUF_ASM_INST_CC_NE)
                                    .map(LEXY_LIT("lt"), LAUF_ASM_INST_CC_LT)
                                    .map(LEXY_LIT("le"), LAUF_ASM_INST_CC_LE)
                                    .map(LEXY_LIT("le"), LAUF_ASM_INST_CC_GT)
                                    .map(LEXY_LIT("gt"), LAUF_ASM_INST_CC_GE);

    static constexpr auto rule  = LAUF_KEYWORD("cc") >> dsl::symbol<ccs>;
    static constexpr auto value = inst(&lauf_asm_inst_cc);
};

struct inst_stack_op
{
    static constexpr auto insts = lexy::symbol_table<void(*)(lauf_asm_builder*, std::uint16_t)>
        .map(LEXY_LIT("pop"), &lauf_asm_inst_pop)
        .map(LEXY_LIT("pick"), &lauf_asm_inst_pick)
        .map(LEXY_LIT("roll"), &lauf_asm_inst_roll)
        .map(LEXY_LIT("select"), &lauf_asm_inst_select);

    static constexpr auto rule
        = dsl::symbol<insts>(identifier::unquoted) >> dsl::integer<std::uint16_t>;
    static constexpr auto value = inst();
};

struct inst_call
{
    static constexpr auto rule  = LAUF_KEYWORD("call") >> dsl::p<function_ref>;
    static constexpr auto value = inst(&lauf_asm_inst_call);
};
struct inst_call_indirect
{
    static constexpr auto rule  = LAUF_KEYWORD("call_indirect") >> dsl::p<signature>;
    static constexpr auto value = inst(&lauf_asm_inst_call_indirect);
};
struct inst_call_builtin
{
    static constexpr auto rule  = dsl::p<builtin_ref>;
    static constexpr auto value = inst(&lauf_asm_inst_call_builtin);
};

struct inst_fiber_resume
{
    static constexpr auto rule  = LAUF_KEYWORD("fiber_resume") >> dsl::p<signature>;
    static constexpr auto value = inst(&lauf_asm_inst_fiber_resume);
};
struct inst_fiber_transfer
{
    static constexpr auto rule  = LAUF_KEYWORD("fiber_transfer") >> dsl::p<signature>;
    static constexpr auto value = inst(&lauf_asm_inst_fiber_transfer);
};
struct inst_fiber_suspend
{
    static constexpr auto rule  = LAUF_KEYWORD("fiber_suspend") >> dsl::p<signature>;
    static constexpr auto value = inst(&lauf_asm_inst_fiber_suspend);
};

struct inst_array_element
{
    static constexpr auto rule  = LAUF_KEYWORD("array_element") >> dsl::p<layout_expr>;
    static constexpr auto value = inst(&lauf_asm_inst_array_element);
};
struct inst_aggregate_member
{
    struct aggregate
    {
        static constexpr auto rule
            = dsl::curly_bracketed.list(dsl::p<layout_expr>, dsl::sep(dsl::comma));
        static constexpr auto value = lexy::as_list<std::vector<lauf_asm_layout>>;
    };

    static constexpr auto rule
        = LAUF_KEYWORD("aggregate_member") >> dsl::p<aggregate> + dsl::integer<std::size_t>;
    static constexpr auto value = inst(
        [](lauf_asm_builder* b, const std::vector<lauf_asm_layout>& members, std::size_t index) {
            lauf_asm_inst_aggregate_member(b, index, members.data(), members.size());
        });
};
struct inst_load_field
{
    static constexpr auto rule
        = LAUF_KEYWORD("load_field") >> dsl::p<type_ref> + dsl::integer<std::size_t>;
    static constexpr auto value = inst(&lauf_asm_inst_load_field);
};
struct inst_store_field
{
    static constexpr auto rule
        = LAUF_KEYWORD("store_field") >> dsl::p<type_ref> + dsl::integer<std::size_t>;
    static constexpr auto value = inst(&lauf_asm_inst_store_field);
};

struct location
{
    static constexpr auto rule  = dsl::position;
    static constexpr auto value = callback([](parse_state& state, auto pos) {
        auto loc = lexy::get_input_location(state.input->buffer, pos, state.anchor);
        lauf_asm_build_debug_location(state.builder, {std::uint16_t(loc.line_nr()),
                                                      std::uint16_t(loc.column_nr()), false});
        state.anchor = loc.anchor();
    });
};

struct instruction
{
    static constexpr auto rule = [] {
        auto nested = dsl::square_bracketed.list(dsl::recurse<instruction>);

        auto single
            = dsl::p<inst_return> | dsl::p<inst_panic>                                     //
              | dsl::p<inst_jump> | dsl::p<inst_branch>                                    //
              | dsl::p<inst_sint> | dsl::p<inst_uint>                                      //
              | dsl::p<inst_null> | dsl::p<inst_global_addr> | dsl::p<inst_local_addr>     //
              | dsl::p<inst_function_addr> | dsl::p<inst_layout> | dsl::p<inst_cc>         //
              | dsl::p<inst_stack_op>                                                      //
              | dsl::p<inst_call> | dsl::p<inst_call_indirect> | dsl::p<inst_call_builtin> //
              | dsl::p<inst_fiber_resume> | dsl::p<inst_fiber_transfer>                    //
              | dsl::p<inst_fiber_suspend>                                                 //
              | dsl::p<inst_array_element> | dsl::p<inst_aggregate_member>                 //
              | dsl::p<inst_load_field> | dsl::p<inst_store_field>;

        return nested | dsl::else_ >> dsl::p<location> + single + dsl::semicolon;
    }();

    static constexpr auto value = lexy::forward<void>;
};

//=== function ===//
struct block
{
    struct header
    {
        static constexpr auto rule
            = LAUF_KEYWORD("block") + dsl::p<local_identifier> + dsl::p<signature>;
        static constexpr auto value
            = callback([](parse_state& state, const std::string& name, lauf_asm_signature sig) {
                  lauf_asm_block* block;
                  if (auto b = state.blocks.try_lookup(name))
                  {
                      block = *b;
                  }
                  else
                  {
                      block = lauf_asm_declare_block(state.builder, sig.input_count);
                      state.blocks.insert(name, block);
                  }

                  lauf_asm_build_block(state.builder, block);
              });
    };

    static constexpr auto rule  = dsl::p<header> + dsl::curly_bracketed.list(dsl::p<instruction>);
    static constexpr auto value = lexy::forward<void>;
};

struct local_decl
{
    static constexpr auto rule
        = LAUF_KEYWORD("local") >> dsl::position + dsl::p<local_identifier> + dsl::colon
                                       + dsl::p<layout_expr> + dsl::semicolon;
    static constexpr auto value = callback(
        [](parse_state& state, auto pos, const std::string& name, lauf_asm_layout layout) {
            auto local = lauf_asm_build_local(state.builder, layout);
            if (!state.locals.insert(name, local))
                state.duplicate_declaration(pos, "local", name.c_str());
        });
};

struct function_decl
{
    struct header
    {
        static constexpr auto rule = dsl::position + dsl::p<global_identifier> + dsl::p<signature>;

        static constexpr auto value = callback([](parse_state& state, auto pos,
                                                  const std::string& name, lauf_asm_signature sig) {
            if (auto f = state.functions.try_lookup(name))
            {
                auto f_sig = lauf_asm_function_signature(*f);
                if (f_sig.input_count != sig.input_count || f_sig.output_count != sig.output_count)
                    state.conflicting_signature(pos, "function", name.c_str());
                state.fn = *f;
            }
            else
            {
                state.fn = lauf_asm_add_function(state.mod, name.c_str(), sig);
                state.functions.insert(name, state.fn);
            }

            lauf_asm_build(state.builder, state.mod, state.fn);
            state.blocks.clear();
            state.locals.clear();
        });
    };

    struct export_
    {
        static constexpr auto rule = LEXY_LIT("export");
        static constexpr auto value
            = callback([](const parse_state& state) { lauf_asm_export_function(state.fn); });
    };

    struct body
    {
        static void create_entry_block(parse_state& state)
        {
            auto block = lauf_asm_declare_block(state.builder,
                                                lauf_asm_function_signature(state.fn).input_count);
            lauf_asm_build_block(state.builder, block);
            state.blocks.insert("", block);
        }

        static constexpr auto rule = [] {
            auto block_list = dsl::peek(LAUF_KEYWORD("block"))
                              >> dsl::curly_bracketed.as_terminator().list(dsl::p<block>);
            auto inst_list = dsl::effect<create_entry_block> //
                             + dsl::curly_bracketed.as_terminator().list(dsl::p<instruction>);

            auto locals = dsl::if_(dsl::list(dsl::p<local_decl>));

            return dsl::curly_bracketed.open() >> locals + (block_list | dsl::else_ >> inst_list);
        }();

        static constexpr auto value = lexy::noop >> callback([](const parse_state& state) {
                                          lauf_asm_build_finish(state.builder);
                                      });
    };

    static constexpr auto rule
        = LAUF_KEYWORD("function")
          >> dsl::p<header> + dsl::if_(dsl::p<export_>) + (dsl::semicolon | dsl::p<body>);
    static constexpr auto value = lexy::forward<void>;
};

//=== module ===//
struct module_decl
{
    static constexpr auto whitespace
        = dsl::ascii::space | dsl::hash_sign >> dsl::until(dsl::newline);

    struct header
    {
        static constexpr auto rule
            = LAUF_KEYWORD("module") + dsl::p<global_identifier> + dsl::semicolon;
        static constexpr auto value = callback([](parse_state& state, const std::string& name) {
            state.mod = lauf_asm_create_module(name.c_str());
        });
    };

    static constexpr auto rule
        = dsl::p<header> //
          + dsl::terminator(dsl::eof).opt_list(dsl::p<global_decl> | dsl::p<function_decl>);
    static constexpr auto value = lexy::forward<void>;
};
} // namespace lauf::text_grammar

lauf_asm_module* lauf_frontend_text(lauf_reader* reader, lauf_frontend_text_options opts)
{
    parse_state state(reader, opts);

    auto callback = state.report_error();
    auto result   = lexy::parse<lauf::text_grammar::module_decl>(reader->buffer, state, callback);
    if (!result)
    {
        if (state.mod != nullptr)
            lauf_asm_destroy_module(state.mod);
        state.mod = nullptr;
    }

    lauf_asm_destroy_builder(state.builder);

    if (auto path = reader->path)
        lauf_asm_set_module_debug_path(state.mod, path);

    return state.mod;
}

