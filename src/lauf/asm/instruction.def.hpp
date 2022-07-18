//=== control flow ===//
// Does nothing.
LAUF_ASM_INST(nop, asm_inst_none)

// lauf_asm_inst_return()
LAUF_ASM_INST(return_, asm_inst_none)

// lauf_asm_inst_jump()
LAUF_ASM_INST(jump, asm_inst_offset)

// lauf_asm_inst_branch2(): jumps if false, fallthrough otherwise
// Consumes condition in either case.
LAUF_ASM_INST(branch_false, asm_inst_offset)

// lauf_asm_inst_branch3(): jumps if equal, fallthrough otherwise
// Does not consume condition on fallthrough.
LAUF_ASM_INST(branch_eq, asm_inst_offset)
// lauf_asm_inst_branch3(): jumps if greater, fallthrough otherwise
// Consumes condition.
// Invariant: preceded by branch_eq.
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

//=== stack manipulation ===//
// lauf_asm_inst_pop()
LAUF_ASM_INST(pop, asm_inst_stack_idx)

// lauf_asm_inst_pick()
LAUF_ASM_INST(pick, asm_inst_stack_idx)

// lauf_asm_inst_roll()
LAUF_ASM_INST(roll, asm_inst_stack_idx)

