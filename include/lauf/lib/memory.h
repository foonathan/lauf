// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_LIB_MEMORY_H_INCLUDED
#define LAUF_LIB_MEMORY_H_INCLUDED

#include <lauf/builtin.h>
#include <lauf/config.h>

LAUF_HEADER_START

// Splits an address into an (allocation, offset) pair.
// address => allocation offset
lauf_builtin lauf_address_to_int_builtin(void);

// Converst an (allocation, offset) pair back into an address.
// allocation offset => address
lauf_builtin lauf_address_from_int_builtin(void);

// Allocates new heap memory.
// size alignment => addr
lauf_builtin lauf_heap_alloc_builtin(void);
// Frees the heap allocation the address is in.
// addr => _
lauf_builtin lauf_free_alloc_builtin(void);

// Splits a memory allocation after length bytes.
// length base_addr => addr1 addr2
lauf_builtin lauf_split_alloc_builtin(void);
// Merges two split memory allocations.
// addr1 addr2 => base_addr
lauf_builtin lauf_merge_alloc_builtin(void);

// Poisons the memory allocation the address is in.
// addr => _
lauf_builtin lauf_poison_alloc_builtin(void);
// Unoisons the memory allocation the address is in.
// addr => _
lauf_builtin lauf_unpoison_alloc_builtin(void);

LAUF_HEADER_END

#endif // LAUF_LIB_MEMORY_H_INCLUDED

