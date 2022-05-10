// Does nothing.
LAUF_BC_OP(nop, bc_inst_none)

// Return from current function.
LAUF_BC_OP(return_, bc_inst_none)
// Increments ip by offset.
LAUF_BC_OP(jump, bc_inst_offset)
// Increments ip by offset if cc matches.
LAUF_BC_OP(jump_if, bc_inst_cc_offset)

// Push literal from table.
// _ => literal
LAUF_BC_OP(push, bc_inst_literal_idx)
// Push zero.
// _ => 0
LAUF_BC_OP(push_zero, bc_inst_none)
// Push small literal from payload, zero extending it.
// _ => literal
LAUF_BC_OP(push_small_zext, bc_inst_literal)
// Push small literal from payload, negating it.
// _ => literal
LAUF_BC_OP(push_small_neg, bc_inst_literal)

// Push the address of a local variable, literal is address relative to function local begin.
// _ => (local_base_addr + literal)
LAUF_BC_OP(local_addr, bc_inst_literal)
// Computes the address of an array element, literal is elem_size.
// idx addr => (addr + elem_size * idx)
LAUF_BC_OP(array_element, bc_inst_literal)

// Drops n values from stack.
// b an ... a1 => b
LAUF_BC_OP(drop, bc_inst_literal)
// Duplicates the nth item on top of the stack.
// an ... a1 => an ... a1 an
LAUF_BC_OP(pick, bc_inst_literal)
// Duplicates the item on top of the stack (pick 0)
// a => a a
LAUF_BC_OP(dup, bc_inst_none)
// Moves the nth item to the top of the stack.
// an ... a1 => a(n-1) ... a1 an
LAUF_BC_OP(roll, bc_inst_literal)
// Swaps the top two items of the stack (roll 1)
// b a => a b
LAUF_BC_OP(swap, bc_inst_none)

// Call function.
// in => out
LAUF_BC_OP(call, bc_inst_function_idx)
// Call builtin, literal is lauf_builtin_function.
// in => out
LAUF_BC_OP(call_builtin, bc_inst_literal_idx)

// Load a field from a type, literal is lauf_type*.
// addr => value
LAUF_BC_OP(load_field, bc_inst_field_literal_idx)
// Store a field to a type, literal is lauf_type*.
// value addr => _
LAUF_BC_OP(store_field, bc_inst_field_literal_idx)
// Save a field from a type, literal is lauf_type*.
// value addr => value
LAUF_BC_OP(save_field, bc_inst_field_literal_idx)

// Load a stack value from a literal address.
// _ => value
LAUF_BC_OP(load_value, bc_inst_literal)
// Store a stack value to a literal address.
// value => _
LAUF_BC_OP(store_value, bc_inst_literal)
// Save a stack value to a literal address.
// value => value
LAUF_BC_OP(save_value, bc_inst_literal)

