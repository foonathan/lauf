// Does nothing.
LAUF_BC_OP(nop, bc_inst_none)

// Return from current function.
LAUF_BC_OP(return_, bc_inst_none)
// Increments ip by offset.
LAUF_BC_OP(jump, bc_inst_offset)
// Increments ip by offset if cc matches.
LAUF_BC_OP(jump_if, bc_inst_cc_offset)

// Push constant from table.
// _ => constant
LAUF_BC_OP(push, bc_inst_constant_idx)
// Push zero.
// _ => 0
LAUF_BC_OP(push_zero, bc_inst_none)
// Push small constant from payload, zero extending it.
// _ => constant
LAUF_BC_OP(push_small_zext, bc_inst_constant)
// Push small constant from payload, negating it.
// _ => constant
LAUF_BC_OP(push_small_neg, bc_inst_constant)

// Pushes nth argument.
// _ => arg
LAUF_BC_OP(argument, bc_inst_constant)
// Push the address of a local variable, constant is address relative to function local begin.
// _ => (local_base_addr + constant)
LAUF_BC_OP(local_addr, bc_inst_constant)

// Drops n values from stack.
// b an ... a1 => b
LAUF_BC_OP(drop, bc_inst_constant)
// Duplicates the nth item on top of the stack.
// an ... a1 => an ... a1 an
LAUF_BC_OP(pick, bc_inst_constant)
// Duplicates the item on top of the stack (pick 0)
// a => a a
LAUF_BC_OP(dup, bc_inst_none)
// Moves the nth item to the top of the stack.
// an ... a1 => a(n-1) ... a1 an
LAUF_BC_OP(roll, bc_inst_constant)
// Swaps the top two items of the stack (roll 1)
// b a => a b
LAUF_BC_OP(swap, bc_inst_none)

// Call function.
// in => out
LAUF_BC_OP(call, bc_inst_function_idx)
// Call builtin, constant is lauf_builtin_function.
// in => out
LAUF_BC_OP(call_builtin, bc_inst_constant_idx)

// Load a field from a type, constant is lauf_type*.
// addr => value
LAUF_BC_OP(load_field, bc_inst_field_constant_idx)
// Store a field to a type, constant is lauf_type*.
// value addr => _
LAUF_BC_OP(store_field, bc_inst_field_constant_idx)

