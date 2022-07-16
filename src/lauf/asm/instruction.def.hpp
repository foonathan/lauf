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
LAUF_ASM_INST(branch_gt, asm_inst_offset)

// lauf_asm_inst_panic()
LAUF_ASM_INST(panic, asm_inst_none)

