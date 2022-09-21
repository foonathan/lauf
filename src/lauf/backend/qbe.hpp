// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_BACKEND_QBE_HPP_INCLUDED
#define SRC_LAUF_BACKEND_QBE_HPP_INCLUDED

#include <lauf/backend/qbe.h>

#include <cinttypes>
#include <lauf/writer.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace lauf
{
enum class qbe_type
{
    word,
    long_,
    single,
    double_,
    byte,
    halfword,

    value = long_,
};

struct qbe_void
{};

enum class qbe_tuple : unsigned
{
};

using qbe_abi_type = std::variant<qbe_void, qbe_type, qbe_tuple>;

enum class qbe_data : unsigned
{
};

enum class qbe_literal : unsigned
{
};

enum class qbe_reg : unsigned
{
    tmp  = unsigned(-1),
    tmp2 = unsigned(-2),
};

enum class qbe_alloc : unsigned
{
    return_ = unsigned(-1),
};

using qbe_value
    = std::variant<qbe_reg, qbe_alloc, std::uintmax_t, qbe_data, qbe_literal, const char*>;

enum class qbe_block : std::size_t
{
};

enum class qbe_cc
{
    ieq,
    ine,

    sle,
    slt,
    sge,
    sgt,

    ule,
    ult,
    uge,
    ugt,
};

class qbe_writer
{
public:
    qbe_writer() : _writer(lauf_create_string_writer()) {}

    qbe_writer(const qbe_writer&)            = delete;
    qbe_writer& operator=(const qbe_writer&) = delete;

    void finish(lauf_writer* out) &&
    {
        for (auto& [str, id] : _literals)
        {
            out->format("data $lit_%u = {", id);
            for (auto c : str)
                out->format("b %d, ", c);
            out->write("}\n");
        }

        for (auto size : _tuples)
        {
            out->format("type :tuple_%u = { %s %u }\n", size, type_name(lauf::qbe_type::value),
                        size);
        }

        out->write("\n");
        out->write(lauf_writer_get_string(_writer));
        lauf_destroy_writer(_writer);
    }

    //=== type ===//
    qbe_tuple tuple(unsigned size)
    {
        _tuples.insert(size);
        return qbe_tuple{size};
    }

    //=== linkage ===//
    void export_()
    {
        _writer->write("export\n");
    }

    //=== data ===//
    void begin_data(qbe_data id, std::size_t alignment)
    {
        _writer->format("data $data_%u = align %zu\n", id, alignment);
        _writer->write("{\n");
    }

    void data_item(qbe_type ty, std::uintmax_t data)
    {
        _writer->format("  %s %" PRIuMAX ",\n", type_name(ty), data);
    }

    void data_item(std::size_t size)
    {
        _writer->format("  z %zu,\n", size);
    }

    void end_data()
    {
        _writer->write("}\n\n");
    }

    //=== string literal ===//
    qbe_literal literal(const char* str)
    {
        auto next_id = qbe_literal(_literals.size());
        return _literals.emplace(str, next_id).first->second;
    }

    //=== function ===//
    void begin_function(const char* name, qbe_abi_type ty)
    {
        if (std::holds_alternative<qbe_void>(ty))
            _writer->format("function $%s(", name);
        else if (std::holds_alternative<qbe_type>(ty))
            _writer->format("function %s $%s(", type_name(std::get<qbe_type>(ty)), name);
        else
            _writer->format("function :tuple_%u $%s(", std::get<qbe_tuple>(ty), name);
    }

    void param(qbe_type ty, std::size_t idx)
    {
        _writer->format("%s %%r%zu,", type_name(ty), idx);
    }

    void body()
    {
        _writer->write(")\n");
        _writer->write("{\n");
        _writer->write("@entry\n");
    }

    void block(qbe_block id)
    {
        _writer->format("@block_%zu\n", id);
    }

    void end_function()
    {
        _writer->write("@error\n");
        _writer->write("  jmp @error\n");
        _writer->write("}\n\n");
    }

    //=== instructions ===//
    void jmp(qbe_block block)
    {
        _writer->format("  jmp @block_%zu\n", block);
    }

    void jnz(qbe_reg reg, qbe_block block1, qbe_block block2)
    {
        _writer->write("  jnz ");
        write_reg(reg);
        _writer->format(", @block_%zu, @block_%zu\n", block1, block2);
    }

    void ret()
    {
        _writer->write("  ret\n");
    }
    void ret(qbe_value value)
    {
        _writer->write("  ret ");
        write_value(value);
        _writer->write("\n");
    }

    void binary_op(qbe_reg dest, qbe_type ty, const char* op, qbe_value lhs, qbe_value rhs)
    {
        _writer->write("  ");
        write_reg(dest);
        _writer->format(" =%s %s ", type_name(ty), op);
        write_value(lhs);
        _writer->write(", ");
        write_value(rhs);
        _writer->write("\n");
    }
    void binary_op(qbe_alloc dest, qbe_type ty, const char* op, qbe_value lhs, qbe_value rhs)
    {
        _writer->write("  ");
        write_alloc(dest);
        _writer->format(" =%s %s ", type_name(ty), op);
        write_value(lhs);
        _writer->write(", ");
        write_value(rhs);
        _writer->write("\n");
    }

    void store(qbe_type ty, qbe_value value, qbe_value ptr)
    {
        _writer->format("  store%s ", type_name(ty));
        write_value(value);
        _writer->write(", ");
        write_value(ptr);
        _writer->write("\n");
    }

    void load(qbe_reg dest, qbe_type ty, qbe_value ptr)
    {
        _writer->write("  ");
        write_reg(dest);
        _writer->format(" =%s load%s ", type_name(ty), type_name(ty));
        write_value(ptr);
        _writer->write("\n");
    }
    void loadsb(qbe_reg dest, qbe_type ty, qbe_value ptr)
    {
        _writer->write("  ");
        write_reg(dest);
        _writer->format(" =%s loadsb ", type_name(ty));
        write_value(ptr);
        _writer->write("\n");
    }
    void loadsh(qbe_reg dest, qbe_type ty, qbe_value ptr)
    {
        _writer->write("  ");
        write_reg(dest);
        _writer->format(" =%s loadsh ", type_name(ty));
        write_value(ptr);
        _writer->write("\n");
    }
    void loadsw(qbe_reg dest, qbe_type ty, qbe_value ptr)
    {
        _writer->write("  ");
        write_reg(dest);
        _writer->format(" =%s loadsw ", type_name(ty));
        write_value(ptr);
        _writer->write("\n");
    }
    void loadub(qbe_reg dest, qbe_type ty, qbe_value ptr)
    {
        _writer->write("  ");
        write_reg(dest);
        _writer->format(" =%s loadub ", type_name(ty));
        write_value(ptr);
        _writer->write("\n");
    }
    void loaduh(qbe_reg dest, qbe_type ty, qbe_value ptr)
    {
        _writer->write("  ");
        write_reg(dest);
        _writer->format(" =%s loaduh ", type_name(ty));
        write_value(ptr);
        _writer->write("\n");
    }
    void loaduw(qbe_reg dest, qbe_type ty, qbe_value ptr)
    {
        _writer->write("  ");
        write_reg(dest);
        _writer->format(" =%s loaduw ", type_name(ty));
        write_value(ptr);
        _writer->write("\n");
    }

    void alloc8(qbe_alloc dest, qbe_value size)
    {
        _writer->write("  ");
        write_alloc(dest);
        _writer->write(" =l alloc8 ");
        write_value(size);
        _writer->write("\n");
    }
    void alloc16(qbe_alloc dest, qbe_value size)
    {
        _writer->write("  ");
        write_alloc(dest);
        _writer->write(" =l alloc16 ");
        write_value(size);
        _writer->write("\n");
    }

    void comparison(qbe_reg dest, qbe_cc cc, qbe_type ty, qbe_value lhs, qbe_value rhs)
    {
        _writer->write("  ");
        write_reg(dest);
        _writer->format(" =l c%s%s ", cc_name(cc), type_name(ty));
        write_value(lhs);
        _writer->write(", ");
        write_value(rhs);
        _writer->write("\n");
    }

    void copy(qbe_reg dest, qbe_type ty, qbe_value value)
    {
        _writer->write("  ");
        write_reg(dest);
        _writer->format(" =%s copy ", type_name(ty));
        write_value(value);
        _writer->write("\n");
    }

    void begin_call(qbe_reg dest, qbe_abi_type ty, qbe_value fn)
    {
        _writer->write("  ");

        if (std::holds_alternative<qbe_void>(ty))
            _writer->write("call ");
        else if (std::holds_alternative<qbe_type>(ty))
        {
            write_reg(dest);
            _writer->format(" =%s call ", type_name(std::get<qbe_type>(ty)));
        }
        else
        {
            write_reg(dest);
            _writer->format(" =:tuple_%u call ", std::get<qbe_tuple>(ty));
        }

        write_value(fn);
        _writer->write("(");
    }

    void argument(qbe_type ty, qbe_value value)
    {
        _writer->format("%s ", type_name(ty));
        write_value(value);
        _writer->write(",");
    }

    void end_call()
    {
        _writer->write(")\n");
    }

    void panic(qbe_value msg)
    {
        begin_call(qbe_reg::tmp, qbe_void(), "lauf_panic");
        argument(qbe_type::value, msg);
        end_call();
    }

private:
    void write_alloc(qbe_alloc a)
    {
        if (a == qbe_alloc::return_)
            _writer->write("%return");
        else
            _writer->format("%%a%u", a);
    }

    void write_reg(qbe_reg reg)
    {
        if (reg == qbe_reg::tmp)
            _writer->write("%tmp1");
        else if (reg == qbe_reg::tmp2)
            _writer->write("%tmp2");
        else
            _writer->format("%%r%u", reg);
    }

    void write_value(const qbe_value& value)
    {
        if (std::holds_alternative<qbe_reg>(value))
            write_reg(std::get<qbe_reg>(value));
        else if (std::holds_alternative<qbe_alloc>(value))
            write_alloc(std::get<qbe_alloc>(value));
        else if (std::holds_alternative<std::uintmax_t>(value))
            _writer->format("%" PRIuMAX, std::get<std::uintmax_t>(value));
        else if (std::holds_alternative<qbe_data>(value))
            _writer->format("$data_%u", std::get<qbe_data>(value));
        else if (std::holds_alternative<qbe_literal>(value))
            _writer->format("$lit_%u", std::get<qbe_literal>(value));
        else
            _writer->format("$%s", std::get<const char*>(value));
    }

    const char* type_name(qbe_type type)
    {
        switch (type)
        {
        case qbe_type::word:
            return "w";
        case qbe_type::long_:
            return "l";
        case qbe_type::single:
            return "s";
        case qbe_type::double_:
            return "d";
        case qbe_type::byte:
            return "b";
        case qbe_type::halfword:
            return "h";
        }
    }

    const char* cc_name(qbe_cc cc)
    {
        switch (cc)
        {
        case qbe_cc::ieq:
            return "eq";
        case qbe_cc::ine:
            return "ne";
        case qbe_cc::sle:
            return "sle";
        case qbe_cc::slt:
            return "slt";
        case qbe_cc::sge:
            return "sge";
        case qbe_cc::sgt:
            return "sgt";
        case qbe_cc::ule:
            return "ule";
        case qbe_cc::ult:
            return "ult";
        case qbe_cc::uge:
            return "uge";
        case qbe_cc::ugt:
            return "ugt";
        }
    }

    lauf_writer*                                 _writer;
    std::unordered_map<std::string, qbe_literal> _literals;
    std::unordered_set<unsigned>                 _tuples;
};
} // namespace lauf

#endif // SRC_LAUF_BACKEND_QBE_HPP_INCLUDED

