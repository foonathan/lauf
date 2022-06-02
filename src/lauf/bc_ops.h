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
    // We're not checking whether it succeeds, as the allocation is either valid and non-destroyed
    // yet, or it was zero sized so is invalid from the start.
    lauf::remove_allocation(process, frame->local_allocation);
    process->allocator.unwind(marker);

    LAUF_DISPATCH;
})

LAUF_BC_OP(call, bc_inst_function_idx, {
    auto callee = process->get_function(ip->call.function_idx);

    auto new_frame_ptr = new_stack_frame(process, frame_ptr, ip + 1, callee);
    if (new_frame_ptr == nullptr)
    {
        auto info = make_panic_info(frame_ptr, ip);
        process->vm->panic_handler(&info, "stack overflow");
        return false;
    }
    frame_ptr = new_frame_ptr;

    auto remaining_vstack_size = vstack_ptr - process->vm->value_stack_limit();
    if (remaining_vstack_size < callee->max_vstack_size)
    {
        auto info = make_panic_info(frame_ptr, ip);
        process->vm->panic_handler(&info, "value stack overflow");
        return false;
    }

    ip = callee->bytecode();
    LAUF_DISPATCH;
})

LAUF_BC_OP(call_builtin, bc_inst_offset_literal_idx, {
    auto callee       = (lauf_builtin_function*)(process->get_literal(ip->call_builtin.literal_idx)
                                               .as_native_ptr);
    auto stack_change = ip->call_builtin.offset;
    ++ip;
    LAUF_DISPATCH_BUILTIN(callee, stack_change);
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

//=== address ===//
// Push the address of a local variable, literal is address relative to function local begin.
// _ => (local_base_addr + literal)
LAUF_BC_OP(local_addr, bc_inst_literal, {
    --vstack_ptr;
    vstack_ptr[0].as_address        = static_cast<stack_frame*>(frame_ptr)[-1].local_allocation;
    vstack_ptr[0].as_address.offset = ip->local_addr.literal;

    ++ip;
    LAUF_DISPATCH;
})

// Push the address of a global variable, literal is allocation index.
LAUF_BC_OP(global_addr, bc_inst_literal, {
    --vstack_ptr;
    vstack_ptr[0].as_address = lauf_value_address{ip->global_addr.literal, 0};

    ++ip;
    LAUF_DISPATCH;
})

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

// Allocates new heap memory.
// size alignment => addr
LAUF_BC_OP(heap_alloc, bc_inst_none, {
    auto vm        = process->vm;
    auto size      = vstack_ptr[1].as_uint;
    auto alignment = vstack_ptr[0].as_uint;
    auto ptr       = vm->allocator.heap_alloc(vm->allocator.user_data, size, alignment);
    if (ptr == nullptr)
    {
        auto info = make_panic_info(frame_ptr, ip);
        vm->panic_handler(&info, "out of heap memory");
        return false;
    }

    auto alloc = lauf::allocation(ptr, uint32_t(size), lauf::allocation::heap_memory);

    ++vstack_ptr;
    vstack_ptr[0].as_address = lauf::add_allocation(process, alloc);

    ++ip;
    LAUF_DISPATCH;
})

// Frees the heap allocation the address is in.
// addr => _
LAUF_BC_OP(free_alloc, bc_inst_none, {
    auto vm   = process->vm;
    auto addr = vstack_ptr[0].as_address;
    ++vstack_ptr;

    auto alloc = process->get_allocation(addr);
    if (alloc == nullptr
        || alloc->source != lauf::allocation::heap_memory
        // We do not allow freeing split memory as others might be using other parts.
        || alloc->split != lauf::allocation::unsplit)
    {
        auto info = make_panic_info(frame_ptr, ip);
        vm->panic_handler(&info, "invalid address");
        return false;
    }
    vm->allocator.free_alloc(vm->allocator.user_data, alloc->ptr);
    lauf::remove_allocation(process, addr);

    ++ip;
    LAUF_DISPATCH;
})

// Splits a memory allocation after length bytes.
// length base_addr => addr1 addr2
LAUF_BC_OP(split_alloc, bc_inst_none, {
    auto length    = vstack_ptr[1].as_uint;
    auto base_addr = vstack_ptr[0].as_address;

    auto base_alloc = process->get_allocation(base_addr);
    if (!base_alloc || length > base_alloc->size)
    {
        auto info = make_panic_info(frame_ptr, ip);
        process->vm->panic_handler(&info, "invalid address");
        return false;
    }

    auto alloc1 = *base_alloc;
    alloc1.size = length;

    auto alloc2 = *base_alloc;
    alloc2.ptr  = base_alloc->offset(length);
    alloc2.size -= length;

    switch (base_alloc->split)
    {
    case lauf::allocation::unsplit:
        alloc1.split = lauf::allocation::first_split;
        alloc2.split = lauf::allocation::last_split;
        break;
    case lauf::allocation::first_split:
        alloc1.split = lauf::allocation::first_split;
        alloc2.split = lauf::allocation::middle_split;
        break;
    case lauf::allocation::middle_split:
        alloc1.split = lauf::allocation::middle_split;
        alloc2.split = lauf::allocation::middle_split;
        break;
    case lauf::allocation::last_split:
        alloc1.split = lauf::allocation::middle_split;
        alloc2.split = lauf::allocation::last_split;
        break;
    }

    *base_alloc  = alloc1;
    auto addr1   = base_addr;
    addr1.offset = 0;
    auto addr2   = add_allocation(process, alloc2);

    vstack_ptr[1].as_address = addr1;
    vstack_ptr[0].as_address = addr2;

    ++ip;
    LAUF_DISPATCH;
})

// Merges two split memory allocations.
// addr1 addr2 => base_addr
LAUF_BC_OP(merge_alloc, bc_inst_none, {
    auto addr1 = vstack_ptr[1].as_address;
    auto addr2 = vstack_ptr[0].as_address;

    auto alloc1 = process->get_allocation(addr1);
    auto alloc2 = process->get_allocation(addr2);
    if (!alloc1
        || !alloc2
        // Both allocations need to be splits.
        || !alloc1->is_split()
        || !alloc2->is_split()
        // And they need to be next to each other.
        || alloc1->offset(alloc1->size) != alloc2->ptr)
    {
        auto info = make_panic_info(frame_ptr, ip);
        process->vm->panic_handler(&info, "invalid address");
        return false;
    }
    auto alloc1_first = alloc1->split == lauf::allocation::first_split;
    auto alloc2_last  = alloc2->split == lauf::allocation::last_split;

    auto& base_alloc = *alloc1;
    base_alloc.size += alloc2->size;
    if (alloc1_first && alloc2_last)
    {
        // If we're merging the first and last split, it's no longer split.
        base_alloc.split = lauf::allocation::unsplit;
    }
    else if (!alloc1_first && alloc2_last)
    {
        // base_alloc is now the last split.
        base_alloc.split = lauf::allocation::last_split;
    }
    auto base_addr = addr1;

    lauf::remove_allocation(process, addr2);

    ++vstack_ptr;
    vstack_ptr[0].as_address = base_addr;

    ++ip;
    LAUF_DISPATCH;
})

// Poisons the memory allocation the address is in.
// addr => _
LAUF_BC_OP(poison_alloc, bc_inst_none, {
    auto addr = vstack_ptr[0].as_address;
    if (auto alloc = process->get_allocation(addr))
    {
        alloc->lifetime = lauf::allocation::poisoned;
    }
    else
    {
        auto info = make_panic_info(frame_ptr, ip);
        process->vm->panic_handler(&info, "invalid address");
        return false;
    }
    --vstack_ptr;

    ++ip;
    LAUF_DISPATCH;
})
// Unpoisons the memory allocation the address is in.
// addr => _
LAUF_BC_OP(unpoison_alloc, bc_inst_none, {
    auto addr = vstack_ptr[0].as_address;
    if (auto alloc = process->get_allocation(addr))
    {
        alloc->lifetime = lauf::allocation::allocated;
    }
    else
    {
        auto info = make_panic_info(frame_ptr, ip);
        process->vm->panic_handler(&info, "invalid address");
        return false;
    }
    --vstack_ptr;

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
    {
        auto info = make_panic_info(frame_ptr, ip);
        process->vm->panic_handler(&info, "select index out of range");
        return false;
    }

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
    auto object = process->get_const_ptr(addr, type->layout.size);
    if (object == nullptr)
    {
        auto info = make_panic_info(frame_ptr, ip);
        process->vm->panic_handler(&info, "invalid address");
        return false;
    }
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
    auto object = process->get_mutable_ptr(addr, type->layout.size);
    if (object == nullptr)
    {
        auto info = make_panic_info(frame_ptr, ip);
        process->vm->panic_handler(&info, "invalid address");
        return false;
    }
    type->store_field(object, ip->store_field.field, vstack_ptr[1]);
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
    {
        auto info = make_panic_info(frame_ptr, ip);
        process->vm->panic_handler(&info, "array index out of bounds");
        return false;
    }

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
    {
        auto info = make_panic_info(frame_ptr, ip);
        process->vm->panic_handler(&info, "array index out of bounds");
        return false;
    }

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

    auto info = make_panic_info(frame_ptr, ip);
    process->vm->panic_handler(&info, process->get_const_cstr(message));

    return false;
})

// Invokes the panic handler if the condition is true.
// value message => _
LAUF_BC_OP(panic_if, bc_inst_cc, {
    auto value   = vstack_ptr[1];
    auto message = vstack_ptr[0].as_address;
    if (check_condition(ip->panic_if.cc, value))
    {
        auto info = make_panic_info(frame_ptr, ip);
        process->vm->panic_handler(&info, process->get_const_cstr(message));
        return false;
    }
    vstack_ptr += 2;

    ++ip;
    LAUF_DISPATCH;
})

