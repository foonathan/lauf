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
    ++vstack_ptr;

    if (check_condition(ip->jump_if.cc, top_value))
        ip += ip->jump_if.offset;

    ++ip;
    LAUF_DISPATCH;
})
// Termination condition for loop with `i != n`.
// (We keep the unused cc field, to patch it.)
LAUF_BC_OP(jump_ifz, bc_inst_cc_offset, {
    auto top_value = vstack_ptr[0];
    ++vstack_ptr;

    if (top_value.as_sint == 0)
        ip += ip->jump_ifz.offset;

    ++ip;
    LAUF_DISPATCH;
})
// Termination condition for loop with `i < n`.
// (We keep the unused cc field, to patch it.)
LAUF_BC_OP(jump_ifge, bc_inst_cc_offset, {
    auto top_value = vstack_ptr[0];
    ++vstack_ptr;

    if (top_value.as_sint >= 0)
        ip += ip->jump_ifge.offset;

    ++ip;
    LAUF_DISPATCH;
})

//=== calls ===//
// Finishes VM execution.
LAUF_BC_OP(exit, bc_inst_none, { return true; })

// Return from current function.
LAUF_BC_OP(return_, bc_inst_none, {
    auto frame  = static_cast<stack_frame*>(frame_ptr) - 1;
    auto marker = frame->unwind;

    frame->free_local_allocations(process);

    ip        = frame->return_ip;
    frame_ptr = frame->prev + 1;
    process->stack().unwind(marker);

    if (frame->prev->fn && frame->prev->fn->jit_fn)
        // We're returning to a JIT function, actual return.
        return true;
    else
        LAUF_DISPATCH;
})
LAUF_BC_OP(return_no_alloc, bc_inst_none, {
    auto frame  = static_cast<stack_frame*>(frame_ptr) - 1;
    auto marker = frame->unwind;

    ip        = frame->return_ip;
    frame_ptr = frame->prev + 1;
    process->stack().unwind(marker);

    if (frame->prev->fn && frame->prev->fn->jit_fn)
        // We're returning to a JIT function, actual return.
        return true;
    else
        LAUF_DISPATCH;
})

// Calls the specified function.
LAUF_BC_OP(call, bc_inst_function_idx, {
    auto callee     = process->get_function(ip->call.function_idx);
    auto marker     = process->stack().top();
    auto prev_frame = static_cast<stack_frame*>(frame_ptr) - 1;

    // As the local_stack_size is a multiple of max alignment, we don't need to worry about aligning
    // it; the builder takes care of it when computing the stack size.
    auto memory = process->stack().try_allocate(sizeof(stack_frame) + callee->local_stack_size);
    if (memory == nullptr)
        // Reserve a new stack block and try again.
        LAUF_TAIL_CALL return reserve_new_stack_block(ip, vstack_ptr, frame_ptr, process);

    auto remaining_vstack_size = vstack_ptr - process->vm()->value_stack_limit();
    if (remaining_vstack_size < callee->max_vstack_size)
        LAUF_DO_PANIC("value stack overflow");

    frame_ptr = new (memory)
                    stack_frame{callee, ip + 1, marker, lauf_value_address_invalid, prev_frame}
                + 1;

    if (callee->jit_fn != nullptr)
    {
        ++ip;
        auto stack_change = int32_t(callee->input_count) - int32_t(callee->output_count);
        LAUF_DISPATCH_BUILTIN(callee->jit_fn, stack_change);
    }
    else
    {
        ip = callee->bytecode();
        LAUF_DISPATCH;
    }
})

// Creates the local allocations for the current function.
LAUF_BC_OP(add_local_allocations, bc_inst_none, {
    auto frame = static_cast<stack_frame*>(frame_ptr) - 1;
    auto fn    = frame->fn;

    if (!process->has_capacity_for_allocations(fn->local_allocation_count))
        // Resize the list and try again.
        LAUF_TAIL_CALL return resize_allocation_list(ip, vstack_ptr, frame_ptr, process);

    auto local_memory = reinterpret_cast<unsigned char*>(frame_ptr);
    frame->first_local_allocation
        = process->add_local_allocations(local_memory, fn->local_allocations(),
                                         fn->local_allocation_count);

    ++ip;
    LAUF_DISPATCH;
})

// Calls the specified builtin function.
LAUF_BC_OP(call_builtin, bc_inst_builtin, {
    auto base_addr = reinterpret_cast<unsigned char*>(&lauf_builtin_finish);
    auto addr      = base_addr + ip->call_builtin.address * std::ptrdiff_t(16);
    auto callee    = reinterpret_cast<lauf_builtin_function*>(addr);

    ++ip;
    LAUF_DISPATCH_BUILTIN(callee, ip->call_builtin.stack_change());
})
LAUF_BC_OP(call_builtin_long, bc_inst_builtin_long, {
    auto addr   = process->get_literal(ip->call_builtin_long.address).as_native_ptr;
    auto callee = (lauf_builtin_function*)addr;

    ++ip;
    LAUF_DISPATCH_BUILTIN(callee, ip->call_builtin_long.stack_change());
})

//=== literals ===//
// Push literal from table.
// _ => literal
LAUF_BC_OP(push, bc_inst_literal_idx, {
    --vstack_ptr;
    vstack_ptr[0] = process->get_literal(ip->push.literal_idx);

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

// Push address, literal is allocation index.
// _ => allocation:0:0
LAUF_BC_OP(push_addr, bc_inst_literal, {
    --vstack_ptr;
    vstack_ptr[0].as_address = lauf_value_address{ip->push_addr.literal, 0};

    ++ip;
    LAUF_DISPATCH;
})

// Push local address, literal is offset from allocation index.
// _ => (first_local_allocation + offset)
LAUF_BC_OP(push_local_addr, bc_inst_literal, {
    auto frame = static_cast<stack_frame*>(frame_ptr) - 1;
    auto addr  = frame->first_local_allocation;
    addr.allocation += ip->push_addr.literal;

    --vstack_ptr;
    vstack_ptr[0].as_address = addr;

    ++ip;
    LAUF_DISPATCH;
})

//=== address ===//
// Computes the address of an array element, literal is elem_size.
// idx addr => (addr + elem_size * idx)
LAUF_BC_OP(array_element_addr, bc_inst_literal, {
    auto idx       = vstack_ptr[1].as_uint;
    auto elem_size = ptrdiff_t(ip->array_element_addr.literal);

    auto addr = vstack_ptr[0].as_address;
    addr.offset += elem_size * idx;

    ++vstack_ptr;
    vstack_ptr[0].as_address = addr;

    ++ip;
    LAUF_DISPATCH;
})

// Computes the address of an aggregate, literal is offset.
// addr => addr + offset
LAUF_BC_OP(aggregate_member_addr, bc_inst_literal, {
    auto addr = vstack_ptr[0].as_address;
    addr.offset += ip->aggregate_member_addr.literal;
    vstack_ptr[0].as_address = addr;

    ++ip;
    LAUF_DISPATCH;
})

//=== value stack manipulation ===//
// Pops n values from stack.
// b an ... a1 => b
LAUF_BC_OP(pop, bc_inst_literal, {
    vstack_ptr += ptrdiff_t(ip->pop.literal);

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
    std::memmove(vstack_ptr + 1, vstack_ptr, index * sizeof(lauf_value));
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

// Selects the nth item from the stack, where the index is passed dynamically.
// Literal N is maximal value for n.
// n aN ... an ... a1 => an
LAUF_BC_OP(select, bc_inst_literal, {
    auto max_index = ptrdiff_t(ip->select.literal);
    auto index     = vstack_ptr[max_index].as_uint;
    if (index >= max_index)
        LAUF_DO_PANIC("select index out of range");

    auto value = vstack_ptr[index];

    vstack_ptr += max_index;
    vstack_ptr[0] = value;

    ++ip;
    LAUF_DISPATCH;
})
// Same as above, but N is 2.
LAUF_BC_OP(select2, bc_inst_none, {
    auto index = vstack_ptr[2].as_uint & 0x1;
    auto value = vstack_ptr[index];

    vstack_ptr += 2;
    vstack_ptr[0] = value;

    ++ip;
    LAUF_DISPATCH;
})

// Selects between one of two values based on the condition code of the third.
// condition if_true if_false => (if_true or if_false)
LAUF_BC_OP(select_if, bc_inst_cc, {
    auto condition = vstack_ptr[2];
    auto value     = check_condition(ip->select_if.cc, condition) ? vstack_ptr[1] : vstack_ptr[0];

    vstack_ptr += 2;
    vstack_ptr[0] = value;

    ++ip;
    LAUF_DISPATCH;
})

//=== load/store ===//
// Load a field from a type, literal is lauf_type*.
// addr => value
LAUF_BC_OP(load_field, bc_inst_field_literal_idx, {
    auto type
        = static_cast<lauf_type>(process->get_literal(ip->load_field.literal_idx).as_native_ptr);

    auto addr   = vstack_ptr[0].as_address;
    auto object = process->get_const_ptr(addr, type->layout);
    if (object == nullptr)
        LAUF_DO_PANIC("invalid address");

    vstack_ptr[0] = type->load_field(object, ip->load_field.field);

    ++ip;
    LAUF_DISPATCH;
})

// Store a field to a type, literal is lauf_type*.
// value addr => _
LAUF_BC_OP(store_field, bc_inst_field_literal_idx, {
    auto type
        = static_cast<lauf_type>(process->get_literal(ip->store_field.literal_idx).as_native_ptr);

    auto addr   = vstack_ptr[0].as_address;
    auto object = process->get_mutable_ptr(addr, type->layout);
    if (object == nullptr)
        LAUF_DO_PANIC("invalid address");

    if (!type->store_field(object, ip->store_field.field, vstack_ptr[1]))
        LAUF_DO_PANIC("invalid field value");
    vstack_ptr += 2;

    ++ip;
    LAUF_DISPATCH;
})

// Load a stack value from a local address.
// _ => value
LAUF_BC_OP(load_value, bc_inst_literal, {
    auto object = static_cast<unsigned char*>(frame_ptr) + ptrdiff_t(ip->load_value.literal);

    --vstack_ptr;
    vstack_ptr[0] = *reinterpret_cast<lauf_value*>(object);

    ++ip;
    LAUF_DISPATCH;
})
// Load a stack value from a local array.
// idx => value
LAUF_BC_OP(load_array_value, bc_inst_literal, {
    auto offset = ptrdiff_t(ip->load_value.literal);
    auto array  = static_cast<unsigned char*>(frame_ptr) + offset;
    auto idx    = vstack_ptr[0].as_uint;

    auto frame = static_cast<stack_frame*>(frame_ptr) - 1;
    if (offset + (idx + 1) * sizeof(lauf_value) > frame->fn->local_stack_size)
        LAUF_DO_PANIC("array index out of bounds");

    vstack_ptr[0] = reinterpret_cast<lauf_value*>(array)[idx];

    ++ip;
    LAUF_DISPATCH;
})

// Store a stack value to a local address.
// value => _
LAUF_BC_OP(store_value, bc_inst_literal, {
    auto object = static_cast<unsigned char*>(frame_ptr) + ptrdiff_t(ip->store_value.literal);

    ::new (object) lauf_value(vstack_ptr[0]);
    ++vstack_ptr;

    ++ip;
    LAUF_DISPATCH;
})
// Store a stack value from a local array.
// value idx => _
LAUF_BC_OP(store_array_value, bc_inst_literal, {
    auto offset = ptrdiff_t(ip->load_value.literal);
    auto array  = static_cast<unsigned char*>(frame_ptr) + offset;
    auto idx    = vstack_ptr[0].as_uint;

    auto frame = static_cast<stack_frame*>(frame_ptr) - 1;
    if (offset + (idx + 1) * sizeof(lauf_value) > frame->fn->local_stack_size)
        LAUF_DO_PANIC("array index out of bounds");

    ::new (array + sizeof(lauf_value) * idx) lauf_value(vstack_ptr[1]);
    vstack_ptr += 2;

    ++ip;
    LAUF_DISPATCH;
})

// Save a stack value to a local address.
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
    auto message = vstack_ptr[0].as_address;
    LAUF_DO_PANIC(process->get_const_cstr(message));
})

// Invokes the panic handler if the condition is true.
// value message => _
LAUF_BC_OP(panic_if, bc_inst_cc, {
    auto value   = vstack_ptr[1];
    auto message = vstack_ptr[0].as_address;
    if (check_condition(ip->panic_if.cc, value))
        LAUF_DO_PANIC(process->get_const_cstr(message));
    vstack_ptr += 2;

    ++ip;
    LAUF_DISPATCH;
})

