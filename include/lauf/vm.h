// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_VM_H_INCLUDED
#define LAUF_VM_H_INCLUDED

#include <lauf/config.h>
#include <lauf/function.h>
#include <lauf/value.h>

LAUF_HEADER_START

typedef struct lauf_VMImpl* lauf_VM;

lauf_VM lauf_vm(void);

void lauf_vm_destroy(lauf_VM vm);

void lauf_vm_execute(lauf_VM vm, lauf_Function fn, const lauf_Value* input, lauf_Value* output);

LAUF_HEADER_END

#endif // LAUF_VM_H_INCLUDED

