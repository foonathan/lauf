//=== control flow ===//
// Does nothing.
LAUF_ASM_INST(nop, asm_inst_none)

// Indicates the beginning of a basic block, but does nothing when executed.
LAUF_ASM_INST(block, asm_inst_signature)

// lauf_asm_inst_return()
LAUF_ASM_INST(return_, asm_inst_none)
// Same, but when it also needs to free N local allocations.
LAUF_ASM_INST(return_free, asm_inst_value)

// lauf_asm_inst_jump()
LAUF_ASM_INST(jump, asm_inst_offset)

// lauf_asm_inst_branch(): jumps if value matches condition code, fallthrough otherwise.,
// Consumes condition in either case.
LAUF_ASM_INST(branch_eq, asm_inst_offset)
LAUF_ASM_INST(branch_ne, asm_inst_offset)
LAUF_ASM_INST(branch_lt, asm_inst_offset)
LAUF_ASM_INST(branch_le, asm_inst_offset)
LAUF_ASM_INST(branch_ge, asm_inst_offset)
LAUF_ASM_INST(branch_gt, asm_inst_offset)

// lauf_asm_inst_panic()
LAUF_ASM_INST(panic, asm_inst_none)

// Exits VM execution; used by its trampoline only.
LAUF_ASM_INST(exit, asm_inst_none)

//=== calls ===//
// lauf_asm_inst_call()
// The offset is the difference between the address of the current function and the called function
// divided by sizeof(void*).
LAUF_ASM_INST(call, asm_inst_offset)

// lauf_asm_inst_call_indirect()
// data is function index
LAUF_ASM_INST(call_indirect, asm_inst_signature)

// lauf_asm_inst_call_builtin()
// The offset is the difference between the address of the lauf_runtime_builtin_dispatch() and the
// called builtin divided by sizeof(void*).
LAUF_ASM_INST(call_builtin, asm_inst_offset)
LAUF_ASM_INST(call_builtin_no_regs, asm_inst_offset)

//=== fibers ===//
// lauf_asm_inst_fiber_resume()
LAUF_ASM_INST(fiber_resume, asm_inst_signature)
// lauf_asm_inst_fiber_transfer()
LAUF_ASM_INST(fiber_transfer, asm_inst_signature)
// lauf_asm_inst_fiber_suspend()
LAUF_ASM_INST(fiber_suspend, asm_inst_signature)

//=== value ===//
// lauf_asm_inst_Xint(): push 24 bit immediate, zero extended to 64 bit.
LAUF_ASM_INST(push, asm_inst_value)

// lauf_asm_inst_sint(): push 24 bit immediate, zero extended to 64 bit, then bit flipped.
LAUF_ASM_INST(pushn, asm_inst_value)

// lauf_asm_inst_Xint(): top |= imm << 24.
// Invariant: preceded by pushn or push.
LAUF_ASM_INST(push2, asm_inst_value)

// lauf_asm_inst_Xint(): top |= imm << 48.
// Invariant: preceded by push2, pushn or push.
LAUF_ASM_INST(push3, asm_inst_value)

// lauf_asm_inst_global_addr(), value is allocation index.
LAUF_ASM_INST(global_addr, asm_inst_value)

// lauf_asm_inst_function_addr()
// The offset is the difference between the address of the current function and the called function
// divided by sizeof(void*).
LAUF_ASM_INST(function_addr, asm_inst_offset)

// lauf_asm_inst_local_addr()
// The value is the index of the local allocation.
LAUF_ASM_INST(local_addr, asm_inst_local_addr)

// lauf_asm_inst_cc()
// The value is the condition code.
LAUF_ASM_INST(cc, asm_inst_value)

//=== stack manipulation ===//
// lauf_asm_inst_pop()
LAUF_ASM_INST(pop, asm_inst_stack_idx)
LAUF_ASM_INST(pop_top, asm_inst_stack_idx)

// lauf_asm_inst_pick()
LAUF_ASM_INST(pick, asm_inst_stack_idx)
LAUF_ASM_INST(dup, asm_inst_stack_idx)

// lauf_asm_inst_roll()
LAUF_ASM_INST(roll, asm_inst_stack_idx)
LAUF_ASM_INST(swap, asm_inst_stack_idx)

// lauf_asm_inst_select()
LAUF_ASM_INST(select, asm_inst_stack_idx)

//=== memory ===//
// Setups a call frame for local allocations.
// Value is the number of local_alloc[_aligned] instructions.
LAUF_ASM_INST(setup_local_alloc, asm_inst_value)
// Allocate memory for a local variable and creates an allocation for it.
// First version assumes alignment of 8, second one allows bigger alignments.
LAUF_ASM_INST(local_alloc, asm_inst_layout)
LAUF_ASM_INST(local_alloc_aligned, asm_inst_layout)
// Allocate memory for local variable but doesn't setup an allocation for it.
// Value is the number of bytes to reserve on the cstack.
LAUF_ASM_INST(local_storage, asm_inst_value)

// lauf_asm_inst_array_element()
// Value is multiple.
LAUF_ASM_INST(array_element, asm_inst_value)
// lauf_asm_inst_aggregate_member()
// Value is offset.
LAUF_ASM_INST(aggregate_member, asm_inst_value)

// Used by lauf_asm_inst_load/store_field()
// Signature: address => native_ptr
LAUF_ASM_INST(deref_const, asm_inst_layout)
LAUF_ASM_INST(deref_mut, asm_inst_layout)

// lauf_asm_inst_load/store_field() for locals and the value type.
// Signature: _ => value
LAUF_ASM_INST(load_local_value, asm_inst_local_addr)
// Signature: value => _
LAUF_ASM_INST(store_local_value, asm_inst_local_addr)

// lauf_asm_inst_load/store_field() for globals and the value type.
// The value is the global allocation index.
// Signature: _ => value
LAUF_ASM_INST(load_global_value, asm_inst_value)
// Signature: value => _
LAUF_ASM_INST(store_global_value, asm_inst_value)

