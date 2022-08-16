// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/vm/jit.hpp>

#include <lauf/asm/module.hpp>

#define LAUF_ASM_INST(Name, Type) extern "C" void lauf_jit_execute_##Name();
#include <lauf/asm/instruction.def.hpp>
#undef LAUF_ASM_INST

extern "C" void lauf_jit_finish();
extern "C" void lauf_jit_last();

namespace
{
const unsigned char* const labels[] = {
#define LAUF_ASM_INST(Name, Type) reinterpret_cast<const unsigned char*>(&lauf_jit_execute_##Name),
#include <lauf/asm/instruction.def.hpp>
#undef LAUF_ASM_INST
    reinterpret_cast<const unsigned char*>(&lauf_jit_last)};
} // namespace

bool lauf::jit_compile(lauf_asm_module* mod, lauf_asm_function* fn)
{
    fn->code = mod->exec_mem.align<alignof(void*)>();
    for (auto i = 0u; i != fn->insts_count; ++i)
    {
        auto op_idx = std::size_t(fn->insts[i].op());
        auto begin  = labels[op_idx];
        auto end    = labels[op_idx + 1];

        mod->exec_mem.allocate(begin, static_cast<std::size_t>(end - begin));
    }
    return true;
}

