// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_IMPL_VM_HPP_INCLUDED
#define SRC_LAUF_IMPL_VM_HPP_INCLUDED

#include <lauf/vm.h>

namespace lauf::_detail
{
size_t frame_size_for(lauf_function fn);

void update_process(lauf_vm vm, lauf_vm_process process);
} // namespace lauf::_detail

#endif // SRC_LAUF_IMPL_VM_HPP_INCLUDED

