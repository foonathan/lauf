//=== jumps ===//
// Does nothing.
LAUF_BC_OP(nop, bc_inst_none, {
    ++ip;
    LAUF_DISPATCH;
})

// Increments ip by offset.
LAUF_BC_OP(jump, bc_inst_offset, {
    ip += ip->jump.offset;
    LAUF_DISPATCH;
})

// Increments ip by offset if cc matches.
LAUF_BC_OP(jump_if, bc_inst_cc_offset, {
    auto top_value = vstack_ptr[0];
    if (check_condition(ip->jump_if.cc, top_value))
        ip += ip->jump_if.offset;
    else
        ++ip;
    ++vstack_ptr;

    LAUF_DISPATCH;
})

//=== calls ===//
// Return from current function.
LAUF_BC_OP(return_, bc_inst_none, {
    auto frame  = static_cast<stack_frame*>(frame_ptr) - 1;
    auto marker = frame->unwind;

    ip        = frame->return_ip;
    frame_ptr = frame->prev + 1;
    vm->memory_stack.unwind(marker);

    LAUF_DISPATCH;
})

LAUF_BC_OP(call, bc_inst_function_idx, {
    auto callee = vm->get_function(ip->call.function_idx);

    auto marker     = vm->memory_stack.top();
    auto prev_frame = static_cast<stack_frame*>(frame_ptr) - 1;

    auto memory = vm->memory_stack.allocate(sizeof(stack_frame) + callee->local_stack_size,
                                            alignof(std::max_align_t));
    auto frame  = ::new (memory) stack_frame{ip + 1, marker, prev_frame};

    ip        = callee->bytecode();
    frame_ptr = frame + 1;

    LAUF_DISPATCH;
})

LAUF_BC_OP(call_builtin, bc_inst_offset_literal_idx, {
    auto callee = (lauf_builtin_function*)(vm->get_literal(ip->call_builtin.literal_idx).as_ptr);
    auto stack_change = ip->call_builtin.offset;
    ++ip;
    LAUF_DISPATCH_BUILTIN(callee, stack_change);
})

//=== literals ===//
// Push literal from table.
// _ => literal
LAUF_BC_OP(push, bc_inst_literal_idx, {
    --vstack_ptr;
    vstack_ptr[0] = vm->get_literal(ip->push.literal_idx);

    ++ip;
    LAUF_DISPATCH;
})

// Push zero.
// _ => 0
LAUF_BC_OP(push_zero, bc_inst_none, {
    --vstack_ptr;
    vstack_ptr[0].as_sint = 0;

    ++ip;
    LAUF_DISPATCH;
})

// Push small literal from payload, zero extending it.
// _ => literal
LAUF_BC_OP(push_small_zext, bc_inst_literal, {
    --vstack_ptr;
    vstack_ptr[0].as_sint = ip->push_small_zext.literal;

    ++ip;
    LAUF_DISPATCH;
})

// Push small literal from payload, negating it.
// _ => -literal
LAUF_BC_OP(push_small_neg, bc_inst_literal, {
    --vstack_ptr;
    vstack_ptr[0].as_sint = -lauf_value_sint(ip->push_small_zext.literal);

    ++ip;
    LAUF_DISPATCH;
})

//=== address ===//
// Push the address of a local variable, literal is address relative to function local begin.
// _ => (local_base_addr + literal)
LAUF_BC_OP(local_addr, bc_inst_literal, {
    --vstack_ptr;
    vstack_ptr[0].as_ptr
        = static_cast<unsigned char*>(frame_ptr) + ptrdiff_t(ip->local_addr.literal);

    ++ip;
    LAUF_DISPATCH;
})

// Computes the address of an array element, literal is elem_size.
// idx addr => (addr + elem_size * idx)
LAUF_BC_OP(array_element, bc_inst_literal, {
    auto idx       = vstack_ptr[1].as_uint;
    auto elem_size = ptrdiff_t(ip->array_element.literal);
    auto addr      = vstack_ptr[0].as_ptr;

    --vstack_ptr;
    vstack_ptr[0].as_ptr = (unsigned char*)(addr) + idx * elem_size;

    ++ip;
    LAUF_DISPATCH;
})

//=== value stack manipulation ===//
// Drops n values from stack.
// b an ... a1 => b
LAUF_BC_OP(drop, bc_inst_literal, {
    vstack_ptr += ptrdiff_t(ip->drop.literal);

    ++ip;
    LAUF_DISPATCH;
})

// Duplicates the nth item on top of the stack.
// an ... a1 => an ... a1 an
LAUF_BC_OP(pick, bc_inst_literal, {
    auto index = ptrdiff_t(ip->pick.literal);
    auto value = vstack_ptr[index];

    --vstack_ptr;
    vstack_ptr[0] = value;

    ++ip;
    LAUF_DISPATCH;
})

// Duplicates the item on top of the stack (pick 0)
// a => a a
LAUF_BC_OP(dup, bc_inst_none, {
    --vstack_ptr;
    vstack_ptr[0] = vstack_ptr[1];

    ++ip;
    LAUF_DISPATCH;
})

// Moves the nth item to the top of the stack.
// an ... a1 => a(n-1) ... a1 an
LAUF_BC_OP(roll, bc_inst_literal, {
    auto index = ptrdiff_t(ip->pick.literal);
    auto value = vstack_ptr[index];
    std::memmove(vstack_ptr + index, vstack_ptr, index * sizeof(lauf_value));
    vstack_ptr[0] = value;

    ++ip;
    LAUF_DISPATCH;
})

// Swaps the top two items of the stack (roll 1)
// b a => a b
LAUF_BC_OP(swap, bc_inst_none, {
    auto tmp      = vstack_ptr[1];
    vstack_ptr[1] = vstack_ptr[0];
    vstack_ptr[0] = tmp;

    ++ip;
    LAUF_DISPATCH;
})

//=== load/store ===//
// Load a field from a type, literal is lauf_type*.
// addr => value
LAUF_BC_OP(load_field, bc_inst_field_literal_idx, {
    auto type = static_cast<lauf_type>(vm->get_literal(ip->load_field.literal_idx).as_ptr);

    auto object   = vstack_ptr[0].as_ptr;
    vstack_ptr[0] = type->load_field(object, ip->load_field.field);

    ++ip;
    LAUF_DISPATCH;
})

// Store a field to a type, literal is lauf_type*.
// value addr => _
LAUF_BC_OP(store_field, bc_inst_field_literal_idx, {
    auto type = static_cast<lauf_type>(vm->get_literal(ip->store_field.literal_idx).as_ptr);

    auto object = const_cast<void*>(vstack_ptr[0].as_ptr);
    type->store_field(object, ip->store_field.field, vstack_ptr[1]);
    vstack_ptr += 2;

    ++ip;
    LAUF_DISPATCH;
})

// Save a field from a type, literal is lauf_type*.
// value addr => value
LAUF_BC_OP(save_field, bc_inst_field_literal_idx, {
    auto type = static_cast<lauf_type>(vm->get_literal(ip->save_field.literal_idx).as_ptr);

    auto object = const_cast<void*>(vstack_ptr[0].as_ptr);
    type->store_field(object, ip->save_field.field, vstack_ptr[1]);
    ++vstack_ptr;

    ++ip;
    LAUF_DISPATCH;
})

// Load a stack value from a literal address.
// _ => value
LAUF_BC_OP(load_value, bc_inst_literal, {
    auto object = static_cast<unsigned char*>(frame_ptr) + ptrdiff_t(ip->load_value.literal);

    --vstack_ptr;
    vstack_ptr[0] = *reinterpret_cast<lauf_value*>(object);

    ++ip;
    LAUF_DISPATCH;
})

// Store a stack value to a literal address.
// value => _
LAUF_BC_OP(store_value, bc_inst_literal, {
    auto object = static_cast<unsigned char*>(frame_ptr) + ptrdiff_t(ip->store_value.literal);

    ::new (object) lauf_value(vstack_ptr[0]);
    ++vstack_ptr;

    ++ip;
    LAUF_DISPATCH;
})

// Save a stack value to a literal address.
// value => value
LAUF_BC_OP(save_value, bc_inst_literal, {
    auto object = static_cast<unsigned char*>(frame_ptr) + ptrdiff_t(ip->save_value.literal);

    ::new (object) lauf_value(vstack_ptr[0]);

    ++ip;
    LAUF_DISPATCH;
})

//=== panic ===//
// Invokes the panic handler.
// message => _
LAUF_BC_OP(panic, bc_inst_none, {
    auto message = static_cast<const char*>(vstack_ptr[0].as_ptr);
    vm->panic_handler(nullptr, message);
    return false;
})

// Invokes the panic handler if the condition is true.
// value message => _
LAUF_BC_OP(panic_if, bc_inst_cc, {
    auto value   = vstack_ptr[1];
    auto message = static_cast<const char*>(vstack_ptr[0].as_ptr);
    if (check_condition(ip->panic_if.cc, value))
    {
        vm->panic_handler(nullptr, message);
        return false;
    }
    vstack_ptr += 2;

    ++ip;
    LAUF_DISPATCH;
})

