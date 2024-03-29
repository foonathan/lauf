// Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/backend/dump.h>

#include <cctype>
#include <lauf/asm/builder.h>
#include <lauf/asm/module.hpp>
#include <lauf/asm/type.h>
#include <lauf/runtime/builtin.h>
#include <lauf/runtime/stack.hpp>
#include <lauf/writer.hpp>
#include <string>

extern "C"
{
    extern const lauf_runtime_builtin_library* lauf_libs;
    extern const size_t                        lauf_libs_count;
}

const lauf_backend_dump_options lauf_backend_default_dump_options = {lauf_libs, lauf_libs_count};

namespace
{
void dump_global(lauf_writer* writer, lauf_backend_dump_options, const lauf_asm_global* global)
{
    writer->write("global ");
    if (global->is_mutable)
        writer->write("mut ");
    else
        writer->write("const ");

    if (auto name = lauf_asm_global_debug_name(global))
        writer->format("@'%s'", name);
    else
        writer->format("@global_%u", global->allocation_idx);

    if (global->has_definition())
    {
        writer->format(": (%zu, %zu) = ", std::size_t(global->size),
                       std::size_t(global->alignment));

        if (global->memory == nullptr)
            writer->write("zero");
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
    }

    writer->write(";\n");
}

std::string find_builtin_name(lauf_backend_dump_options opts, lauf_runtime_builtin_impl* impl)
{
    if (lauf_asm_type_value.load_fn == impl)
        return lauf_asm_type_value.name + std::string(".load");
    else if (lauf_asm_type_value.store_fn == impl)
        return lauf_asm_type_value.name + std::string(".store");

    for (auto i = 0u; i != opts.builtin_libs_count; ++i)
    {
        for (auto builtin = opts.builtin_libs[i].functions; builtin != nullptr;
             builtin      = builtin->next)
            if (builtin->impl == impl)
                return opts.builtin_libs[i].prefix + std::string(".") + builtin->name;

        for (auto type = opts.builtin_libs[i].types; type != nullptr; type = type->next)
            if (type->load_fn == impl)
                return opts.builtin_libs[i].prefix + std::string(".") + type->name
                       + std::string(".load");
            else if (type->store_fn == impl)
                return opts.builtin_libs[i].prefix + std::string(".") + type->name
                       + std::string(".store");
    }

    return "";
}

std::string find_global_name(const lauf_asm_module* mod, unsigned idx)
{
    for (auto global = lauf::get_globals(mod).first; global != nullptr; global = global->next)
        if (global->allocation_idx == idx)
        {
            auto name = lauf_asm_global_debug_name(global);
            if (name != nullptr)
                return "'" + std::string(name) + "'";

            return "global_" + std::to_string(idx);
        }

    return "";
}

void dump_function(lauf_writer* writer, lauf_backend_dump_options opts, const lauf_asm_module* mod,
                   const lauf_asm_function* fn)
{
    writer->format("function @'%s'(%d => %d)", fn->name, fn->sig.input_count, fn->sig.output_count);
    if (fn->insts == nullptr)
    {
        writer->write(";\n");
        return;
    }

    writer->write("\n{\n");

    auto last_debug_location = lauf_asm_debug_location_null;
    for (auto ip = fn->insts; ip != fn->insts + fn->inst_count; ++ip)
    {
        if (ip->op() == lauf::asm_op::block)
        {
            writer->format("<%04zx>(%u => %u):\n", (ip - fn->insts) + 1, ip->block.input_count,
                           ip->block.output_count);
            continue;
        }

        auto debug_location = lauf_asm_find_debug_location_of_instruction(mod, ip);
        if (!lauf_asm_debug_location_eq(last_debug_location, debug_location))
        {
            writer->format("    # at %u:%u:%u%s\n", debug_location.file_id, debug_location.line_nr,
                           debug_location.column_nr,
                           debug_location.is_synthetic ? " [synthetic]" : "");
            last_debug_location = debug_location;
        }

        writer->write("    ");
        switch (ip->op())
        {
        case lauf::asm_op::nop:
            writer->write("nop");
            break;
        case lauf::asm_op::return_:
            writer->write("return");
            break;
        case lauf::asm_op::return_free:
            writer->format("return_free %d", ip->return_free.value);
            break;
        case lauf::asm_op::jump:
            writer->format("jump <%04zx>", ip + ip->jump.offset - fn->insts);
            break;
        case lauf::asm_op::branch_eq:
            writer->format("branch.eq <%04zx>", ip + ip->branch_eq.offset - fn->insts);
            break;
        case lauf::asm_op::branch_ne:
            writer->format("branch.ne <%04zx>", ip + ip->branch_ne.offset - fn->insts);
            break;
        case lauf::asm_op::branch_lt:
            writer->format("branch.lt <%04zx>", ip + ip->branch_lt.offset - fn->insts);
            break;
        case lauf::asm_op::branch_le:
            writer->format("branch.le <%04zx>", ip + ip->branch_le.offset - fn->insts);
            break;
        case lauf::asm_op::branch_ge:
            writer->format("branch.ge <%04zx>", ip + ip->branch_ge.offset - fn->insts);
            break;
        case lauf::asm_op::branch_gt:
            writer->format("branch.gt <%04zx>", ip + ip->branch_gt.offset - fn->insts);
            break;
        case lauf::asm_op::panic:
            writer->write("panic");
            break;
        case lauf::asm_op::panic_if:
            writer->write("panic_if");
            break;
        case lauf::asm_op::exit:
            writer->write("exit");
            break;

        case lauf::asm_op::call: {
            auto callee = lauf::uncompress_pointer_offset<lauf_asm_function>(fn, ip->call.offset);
            writer->format("call @'%s'", callee->name);
            break;
        }
        case lauf::asm_op::call_indirect: {
            writer->write("call_indirect");
            break;
        }
        case lauf::asm_op::call_builtin:
        case lauf::asm_op::call_builtin_no_regs: {
            auto callee = lauf::uncompress_pointer_offset<lauf_runtime_builtin_impl> //
                (&lauf_runtime_builtin_dispatch, ip->call_builtin.offset);
            if (auto name = find_builtin_name(opts, callee); !name.empty())
                writer->format("$'%s'", name.c_str());
            else
                writer->format("$'%p'", reinterpret_cast<void*>(callee));

            if (ip->op() == lauf::asm_op::call_builtin_no_regs)
                writer->write(" [no regs]");

            ++ip; // skip signature
            break;
        }

        case lauf::asm_op::fiber_resume:
            writer->format("fiber_resume (%u => %u)", ip->fiber_resume.input_count,
                           ip->fiber_resume.output_count);
            break;
        case lauf::asm_op::fiber_transfer:
            writer->format("fiber_resume (%u => %u)", ip->fiber_transfer.input_count,
                           ip->fiber_transfer.output_count);
            break;
        case lauf::asm_op::fiber_suspend:
            writer->format("fiber_suspend (%u => %u)", ip->fiber_suspend.input_count,
                           ip->fiber_suspend.output_count);
            break;

        case lauf::asm_op::push:
            writer->format("push 0x%X", ip->push.value);
            break;
        case lauf::asm_op::push2:
            writer->format("push2 0x%X", ip->push2.value);
            break;
        case lauf::asm_op::push3:
            writer->format("push3 0x%X", ip->push3.value);
            break;
        case lauf::asm_op::pushn:
            writer->format("pushn 0x%X", ip->pushn.value);
            break;
        case lauf::asm_op::global_addr: {
            writer->format("global_addr @%s", find_global_name(mod, ip->global_addr.value).c_str());
            break;
        }
        case lauf::asm_op::function_addr: {
            auto callee = lauf::uncompress_pointer_offset<lauf_asm_function>(fn, ip->call.offset);
            writer->format("function_addr @'%s'", callee->name);
            break;
        }
        case lauf::asm_op::local_addr: {
            writer->format("local_addr %u <%zx>", ip->local_addr.index,
                           ip->local_addr.offset - sizeof(lauf_runtime_stack_frame));
            break;
        }
        case lauf::asm_op::cc: {
            switch (lauf_asm_inst_condition_code(ip->cc.value))
            {
            case LAUF_ASM_INST_CC_EQ:
                writer->write("cc eq");
                break;
            case LAUF_ASM_INST_CC_NE:
                writer->write("cc ne");
                break;
            case LAUF_ASM_INST_CC_LT:
                writer->write("cc lt");
                break;
            case LAUF_ASM_INST_CC_LE:
                writer->write("cc le");
                break;
            case LAUF_ASM_INST_CC_GT:
                writer->write("cc gt");
                break;
            case LAUF_ASM_INST_CC_GE:
                writer->write("cc ge");
                break;
            }
            break;
        }

        case lauf::asm_op::pop:
        case lauf::asm_op::pop_top:
            writer->format("pop %d", ip->pop.idx);
            break;
        case lauf::asm_op::pick:
        case lauf::asm_op::dup:
            writer->format("pick %d", ip->pick.idx);
            break;
        case lauf::asm_op::roll:
        case lauf::asm_op::swap:
            writer->format("roll %d", ip->roll.idx);
            break;
        case lauf::asm_op::select:
            writer->format("select %d", ip->select.idx + 1);
            break;

        case lauf::asm_op::setup_local_alloc:
            writer->format("setup_local_alloc %u", ip->setup_local_alloc.value);
            break;
        case lauf::asm_op::local_alloc:
            writer->format("local_alloc (%u, %zu)", ip->local_alloc.size,
                           ip->local_alloc.alignment());
            break;
        case lauf::asm_op::local_alloc_aligned:
            writer->format("local_alloc_aligned (%u, %zu)", ip->local_alloc_aligned.size,
                           ip->local_alloc_aligned.alignment());
            break;
        case lauf::asm_op::local_storage:
            writer->format("local_storage (%u, 8)", ip->local_storage.value);
            break;
        case lauf::asm_op::deref_const:
            writer->format("deref_const (%u, %zu)", ip->deref_const.size,
                           ip->deref_const.alignment());
            break;
        case lauf::asm_op::deref_mut:
            writer->format("deref_mut (%u, %zu)", ip->deref_mut.size, ip->deref_mut.alignment());
            break;
        case lauf::asm_op::array_element:
            writer->format("array_element [%u]", ip->array_element.value);
            break;
        case lauf::asm_op::aggregate_member:
            writer->format("aggregate_member %u", ip->aggregate_member.value);
            break;
        case lauf::asm_op::load_local_value:
            writer->format("load_local_value %u <%zx>", ip->load_local_value.index,
                           ip->load_local_value.offset - sizeof(lauf_runtime_stack_frame));
            break;
        case lauf::asm_op::store_local_value:
            writer->format("store_local_value %u <%zx>", ip->store_local_value.index,
                           ip->store_local_value.offset - sizeof(lauf_runtime_stack_frame));
            break;
        case lauf::asm_op::load_global_value:
            writer->format("load_global_value @%s",
                           find_global_name(mod, ip->load_global_value.value).c_str());
            break;
        case lauf::asm_op::store_global_value:
            writer->format("store_global_value @%s",
                           find_global_name(mod, ip->store_global_value.value).c_str());
            break;

        case lauf::asm_op::count:
        case lauf::asm_op::block:
        case lauf::asm_op::call_builtin_sig:
            assert(false);
            break;
        }
        writer->write(";\n");
    }

    writer->write("}\n");
}

void dump_module_header(lauf_writer* writer, const lauf_asm_module* mod)
{
    writer->format("module @'%s';\n", lauf_asm_module_name(mod));
    if (auto debug_path = lauf_asm_module_debug_path(mod))
        writer->format("debug_path \"%s\";\n", debug_path);
    writer->write("\n");
}
} // namespace

void lauf_backend_dump(lauf_writer* writer, lauf_backend_dump_options options,
                       const lauf_asm_module* mod)
{
    dump_module_header(writer, mod);
    if (auto globals = lauf::get_globals(mod); globals.count > 0)
    {
        for (auto global = globals.first; global != nullptr; global = global->next)
            dump_global(writer, options, global);
        writer->write("\n");
    }

    for (auto function = lauf::get_functions(mod).first; function != nullptr;
         function      = function->next)
    {
        dump_function(writer, options, mod, function);
        writer->write("\n");
    }
}

void lauf_backend_dump_chunk(lauf_writer* writer, lauf_backend_dump_options options,
                             const lauf_asm_module* mod, const lauf_asm_chunk* chunk)
{
    dump_module_header(writer, mod);
    dump_function(writer, options, mod, chunk->fn);
}

