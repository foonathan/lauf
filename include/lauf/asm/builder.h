// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef LAUF_ASM_BUILDER_H_INCLUDED
#define LAUF_ASM_BUILDER_H_INCLUDED

#include <lauf/config.h>

LAUF_HEADER_START

typedef struct lauf_asm_module         lauf_asm_module;
typedef struct lauf_asm_global         lauf_asm_global;
typedef struct lauf_asm_function       lauf_asm_function;
typedef struct lauf_asm_signature      lauf_asm_signature;
typedef struct lauf_asm_debug_location lauf_asm_debug_location;
typedef struct lauf_asm_type           lauf_asm_type;
typedef struct lauf_asm_layout         lauf_asm_layout;
typedef struct lauf_runtime_builtin    lauf_runtime_builtin_function;

typedef enum lauf_asm_inst_condition_code
{
    LAUF_ASM_INST_CC_EQ,
    LAUF_ASM_INST_CC_NE,
    LAUF_ASM_INST_CC_LT,
    LAUF_ASM_INST_CC_LE,
    LAUF_ASM_INST_CC_GT,
    LAUF_ASM_INST_CC_GE,
} lauf_asm_inst_condition_code;

//=== builder ===//
/// Build options.
typedef struct lauf_asm_build_options
{
    /// Handler called when attempting to build an ill-formed body.
    /// If it returns, lauf will attempt to repair the error.
    void (*error_handler)(const char* fn_name, const char* context, const char* msg);
} lauf_asm_build_options;

/// The default build options.
extern const lauf_asm_build_options lauf_asm_default_build_options;

/// Builds code for a function body.
///
/// It internally does some temporary allocations, so it should be re-used when possible.
typedef struct lauf_asm_builder lauf_asm_builder;

lauf_asm_builder* lauf_asm_create_builder(lauf_asm_build_options options);
void              lauf_asm_destroy_builder(lauf_asm_builder* b);

/// Starts building the function body for the specified function.
///
/// If a previous build wasn't finished yet; discards it.
void lauf_asm_build(lauf_asm_builder* b, lauf_asm_module* mod, lauf_asm_function* fn);

/// Finishes building the currently active function body.
///
/// Only at this point will the body be added to the function.
/// Returns true if the body is well-formed, false otherwise.
bool lauf_asm_build_finish(lauf_asm_builder* b);

//=== local variables ===//
/// A local variable within a function.
typedef struct lauf_asm_local lauf_asm_local;

/// Creates a new local variable.
/// They are allocated when the function is initially called, and freed at the end.
lauf_asm_local* lauf_asm_build_local(lauf_asm_builder* b, lauf_asm_layout layout);

//=== blocks ===//
/// A basic block inside a function.
typedef struct lauf_asm_block lauf_asm_block;

/// Declares a new basic block with the specified signature.
///
/// It is only valid inside the current function.
/// If it's the first block, it becomes the entry block.
lauf_asm_block* lauf_asm_declare_block(lauf_asm_builder* b, size_t input_count);

/// Sets the insertion point of the builder to append instruction to the end of the block.
///
/// Blocks don't need to be built at once; the builder can switch between them at will.
void lauf_asm_build_block(lauf_asm_builder* b, lauf_asm_block* block);

/// Returns the current size of the vstack for the active block.
size_t lauf_asm_build_get_vstack_size(lauf_asm_builder* b);

/// Specifies a debug location for a basic block.
///
/// It remains active for all locations added until overriden by a later point.
void lauf_asm_build_debug_location(lauf_asm_builder* b, lauf_asm_debug_location loc);

//=== block terminator instructions ===//
/// Terminator: return from function.
void lauf_asm_inst_return(lauf_asm_builder* b);

/// Terminator: unconditional jump.
void lauf_asm_inst_jump(lauf_asm_builder* b, const lauf_asm_block* dest);

/// Terminator: two-way jump.
///
/// If the top value is non-zero, jumps to `if_true`, otherwise, jumps to `if_false`.
///
/// Signature: condition:uint => _
void lauf_asm_inst_branch2(lauf_asm_builder* b, const lauf_asm_block* if_true,
                           const lauf_asm_block* if_false);

/// Terminator: three-way jump.
///
/// If the top value is < 0, jumps to `if_lt`, if it is `== 0`, jumps to `if_eq`, if it is `> 0`
/// jumps to `if_gt`.
///
/// Signature: condition:sint => _
void lauf_asm_inst_branch3(lauf_asm_builder* b, const lauf_asm_block* if_lt,
                           const lauf_asm_block* if_eq, const lauf_asm_block* if_false);

/// Terminator: panic.
///
/// Invokes the panic handler with the message at the top of the function, and terminates execution.
///
/// Signature: msg:char* => n/a
void lauf_asm_inst_panic(lauf_asm_builder* b);

//=== call instructions ===//
/// Calls the specified function.
///
/// The function must be declared in the same module.
///
/// Signature: in_N ... in_0 => out_M ... out_0
void lauf_asm_inst_call(lauf_asm_builder* b, const lauf_asm_function* callee);

/// Calls the function specified via its address on the vstack.
///
/// Signature: in_N ... in_0 f => out_M ... out_0
void lauf_asm_inst_call_indirect(lauf_asm_builder* b, lauf_asm_signature sig);

/// Calls the specified builtin function.
///
/// Signature: in_N ... in_0 => out_M ... out_0
void lauf_asm_inst_call_builtin(lauf_asm_builder* b, lauf_runtime_builtin_function callee);

//=== fiber instructions ===//
/// Creates a new fiber.
///
/// It starts out suspended.
/// When it is resumed, it starts executing the specified function.
///
/// Signature: _ => handle:fiber
void lauf_asm_inst_fiber_create(lauf_asm_builder* b, const lauf_asm_function* callee);

/// Resumes a suspended fiber.
/// It will execute until its suspension point, and then return back to this location.
///
/// Signature: handle:fiber in_0 ... in_N => handle:fiber out_0 ... out_N
void lauf_asm_inst_fiber_resume(lauf_asm_builder* b, lauf_asm_signature sig);

/// Suspends the current fiber and transfer control back to the fiber that resumed it.
///
/// Signature: in_0 ... in_N => out_0 ... out_N
void lauf_asm_inst_fiber_suspend(lauf_asm_builder* b, lauf_asm_signature sig);

//=== value instructions ===//
/// Pushes an signed integer onto the stack.
///
/// Signature: _ => value:sint
void lauf_asm_inst_sint(lauf_asm_builder* b, lauf_sint value);

/// Pushes an unsigned integer onto the stack.
///
/// Signature: _ => value:uint
void lauf_asm_inst_uint(lauf_asm_builder* b, lauf_uint value);

/// Pushes the null address onto the stack.
///
/// It works for both regular and function addresses.
///
/// Signature: _ => null
void lauf_asm_inst_null(lauf_asm_builder* b);

/// Pushes the address of a global variable onto the stack.
///
/// Signature: _ => global:address
void lauf_asm_inst_global_addr(lauf_asm_builder* b, const lauf_asm_global* global);

/// Pushes the address of a local variable onto the stack.
///
/// Signature: _ => local:address
void lauf_asm_inst_local_addr(lauf_asm_builder* b, const lauf_asm_local* local);

/// Pushes the address of a function onto the stack.
///
/// The function must be declared in the same module.
///
/// Signature: _ => function:function_address
void lauf_asm_inst_function_addr(lauf_asm_builder* b, const lauf_asm_function* function);

/// Pushes the specified layout onto the stack.
///
/// Signature: _ => alignment:uint size:uint
void lauf_asm_inst_layout(lauf_asm_builder* b, lauf_asm_layout layout);

/// Converts the result of a three way comparison into 0 or 1.
///
/// Signature: cmp:sint => bool:uint
void lauf_asm_inst_cc(lauf_asm_builder* b, lauf_asm_inst_condition_code cc);

//=== stack manipulation instructions ===//
/// Pops the Nth value of the stack.
///
/// Signature: x_N+1 x_N x_N-1 ... x_0 => x_N+1 x_N-1 ... x_0
void lauf_asm_inst_pop(lauf_asm_builder* b, uint16_t stack_index);

/// Duplicates the Nth value on top of the stack.
///
/// Signature: x_N+1 x_N x_N-1 ... x_0 => x_N+1 x_N x_N-1 ... x_0 x_N
void lauf_asm_inst_pick(lauf_asm_builder* b, uint16_t stack_index);

/// Moves the Nth value on top of the stack.
///
/// Signature: x_N+1 x_N x_N-1 ... x_0 => x_N+1 x_N-1 ... x_0 x_N
void lauf_asm_inst_roll(lauf_asm_builder* b, uint16_t stack_index);

//=== memory ===//
/// Computes the address of an array element given the element layout.
///
/// Signature: ptr:address index:sint => (ptr + index * layout):address
void lauf_asm_inst_array_element(lauf_asm_builder* b, lauf_asm_layout element_layout);

/// Computes the address of a member of an aggregate with the specified members.
///
/// Signature: ptr:address => (ptr + offset):address
void lauf_asm_inst_aggregate_member(lauf_asm_builder* b, size_t member_index,
                                    const lauf_asm_layout* member_layouts, size_t member_count);

/// Loads a field from a type and pushes it value.
///
/// Signature: ptr:address => value
void lauf_asm_inst_load_field(lauf_asm_builder* b, lauf_asm_type type, size_t field_index);

/// Stores a value into a field of a type.
///
/// Signature: value ptr:address => _
void lauf_asm_inst_store_field(lauf_asm_builder* b, lauf_asm_type type, size_t field_index);

LAUF_HEADER_END

#endif // LAUF_ASM_BUILDER_H_INCLUDED

