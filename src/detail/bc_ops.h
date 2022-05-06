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

// Pops n values from stack.
LAUF_BC_OP(pop, bc_inst_constant)
// Pops 1 value from stack.
LAUF_BC_OP(pop_one, bc_inst_none)

// Call function, constant is lauf_function object.
LAUF_BC_OP(call, bc_inst_constant_idx)
// Call builtin, constant is lauf_builtin_function.
LAUF_BC_OP(call_builtin, bc_inst_constant_idx)
