//=== control flow ===//
// Returns from the function.
LAUF_IR_OP(return_, ir_inst_return)
// Jumps to the specfied basic block.
LAUF_IR_OP(jump, ir_inst_jump)
// Conditionally jumps to one of two basic blocks.
LAUF_IR_OP(branch, ir_inst_branch)

//=== values ===//
// Loads the (function, block) parameter with the specified index into a register.
LAUF_IR_OP(param, ir_inst_index<param_idx>)
// Loads the specified constant into a register.
LAUF_IR_OP(const_, ir_inst_value)

//=== calls ===//
// Calls the specified builtin function.
LAUF_IR_OP(call_builtin, ir_inst_call_builtin)
// Calls the specified function.
LAUF_IR_OP(call, ir_inst_call)
// Specifies a register or constant that is used as argument to a call or jump.
// They're in reverse order, i.e. the first instruction following the call is the one on top of the
// value stack, or the rightmost parameter.
LAUF_IR_OP(argument, ir_inst_argument)
// Placeholder to reserve a register for the result.
LAUF_IR_OP(call_result, ir_inst_none)

//=== locals ===//
// Stores the specified value.
LAUF_IR_OP(store_value, ir_inst_store_value)
// Loads the specified value.
LAUF_IR_OP(load_value, ir_inst_load_value)

//=== arithmetic ===//
// Adds two signed/unsigned integers, wrapping on overflow.
LAUF_IR_OP(iadd, ir_inst_binary)
// Subtracts two signed/unsigned integers, wrapping on overflow.
LAUF_IR_OP(isub, ir_inst_binary)
// Compares two signed integers.
LAUF_IR_OP(scmp, ir_inst_binary)
// Compares two unsigned integers.
LAUF_IR_OP(ucmp, ir_inst_binary)

