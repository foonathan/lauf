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
// Specifies a register or constant that is used as argument.
LAUF_IR_OP(argument, ir_inst_argument)
// Placeholder to reserve a register for the result.
LAUF_IR_OP(call_result, ir_inst_none)

//=== locals ===//
// Stores the specified value.
LAUF_IR_OP(store_value, ir_inst_store_value)
// Loads the specified value.
LAUF_IR_OP(load_value, ir_inst_load_value)

