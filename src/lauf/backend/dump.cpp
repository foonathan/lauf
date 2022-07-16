// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/backend/dump.h>

#include <cctype>
#include <lauf/asm/module.hpp>
#include <lauf/writer.hpp>

const lauf_backend_dump_options lauf_backend_default_dump_options = {};

namespace
{
void dump_global(lauf_writer* writer, lauf_backend_dump_options, const lauf_asm_global* global)
{
    if (global->perms == lauf_asm_global::read_only)
        writer->write("const ");
    else
        writer->write("data ");

    writer->format("@'%p' = ", static_cast<const void*>(global));

    if (global->memory == nullptr)
    {
        writer->format("[00] * %zu", std::size_t(global->size));
    }
    else
    {
        writer->write("[");
        for (auto i = 0u; i != global->size; ++i)
        {
            if (i > 0)
                writer->write(",");

            writer->format("%02X", global->memory[i]);
        }
        writer->write("]");
    }

    writer->write(";\n");
}

void dump_function(lauf_writer* writer, lauf_backend_dump_options, const lauf_asm_function* fn)
{
    writer->format("function @'%s'(%d => %d)", fn->name, fn->sig.input_count, fn->sig.output_count);
    if (fn->insts == nullptr)
    {
        writer->write(";\n");
        return;
    }

    writer->write("\n{\n");

    for (auto ip = fn->insts; ip != fn->insts + fn->insts_count; ++ip)
    {
        writer->format("  <%04zx>: ", ip - fn->insts);
        switch (ip->op())
        {
        case lauf::asm_op::nop:
            writer->write("nop");
            break;
        case lauf::asm_op::return_:
            writer->write("return");
            break;
        case lauf::asm_op::jump:
            writer->format("jump <%04zx>", ip + ip->jump.offset - fn->insts);
            break;
        case lauf::asm_op::branch_false:
            writer->format("branch.false <%04zx>", ip + ip->branch_false.offset - fn->insts);
            break;
        case lauf::asm_op::branch_eq:
            writer->format("branch.eq <%04zx>", ip + ip->branch_eq.offset - fn->insts);
            break;
        case lauf::asm_op::branch_gt:
            writer->format("branch.gt <%04zx>", ip + ip->branch_gt.offset - fn->insts);
            break;
        case lauf::asm_op::panic:
            writer->write("panic");
            break;
        }
        writer->write(";\n");
    }

    writer->write("}\n");
}
} // namespace

void lauf_backend_dump(lauf_writer* writer, lauf_backend_dump_options options,
                       const lauf_asm_module* mod)
{
    writer->format("module @'%s';\n", mod->name);
    writer->write("\n");

    for (auto global = mod->globals; global != nullptr; global = global->next)
        dump_global(writer, options, global);
    writer->write("\n");

    for (auto function = mod->functions; function != nullptr; function = function->next)
    {
        dump_function(writer, options, function);
        writer->write("\n");
    }
}

