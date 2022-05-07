// Does nothing.
LAUF_BC_OP(nop, bc_inst_none)

// Return from current function.
LAUF_BC_OP(return_, bc_inst_none)
// Increments ip by offset.
LAUF_BC_OP(jump, bc_inst_offset)
// Increments ip by offset if cc matches.
LAUF_BC_OP(jump_if, bc_inst_cc_offset)

// Push constant from table.
LAUF_BC_OP(push, bc_inst_constant_idx)
// Push zero.
LAUF_BC_OP(push_zero, bc_inst_none)
// Push small constant from payload, zero extending it.
LAUF_BC_OP(push_small_zext, bc_inst_constant)
// Push small constant from payload, negating it.
LAUF_BC_OP(push_small_neg, bc_inst_constant)

// Pushes nth argument.
LAUF_BC_OP(argument, bc_inst_constant)

// Drops n values from stack.
LAUF_BC_OP(drop, bc_inst_constant)
// Duplicates the nth item on top of the stack.
LAUF_BC_OP(pick, bc_inst_constant)
// Duplicates the item on top of the stack (pick 0)
LAUF_BC_OP(dup, bc_inst_none)
// Moves the nth item to the top of the stack.
LAUF_BC_OP(roll, bc_inst_constant)
// Swaps the top two items of the stack (roll 1)
LAUF_BC_OP(swap, bc_inst_none)

// Call function.
LAUF_BC_OP(call, bc_inst_function_idx)
// Call builtin, constant is lauf_builtin_function.
LAUF_BC_OP(call_builtin, bc_inst_constant_idx)
