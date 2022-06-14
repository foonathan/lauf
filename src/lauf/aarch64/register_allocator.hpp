// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_AARCH64_REGISTER_ALLOCATOR_HPP_INCLUDED
#define SRC_LAUF_AARCH64_REGISTER_ALLOCATOR_HPP_INCLUDED

#include <algorithm>
#include <cassert>
#include <lauf/aarch64/emitter.hpp>
#include <lauf/config.h>
#include <vector>

namespace lauf::aarch64
{
class register_allocator
{
    struct vstack
    {};

    // Where a value is currently stored:
    // * in a specific register
    // * on the vstack
    struct location
    {
        std::uint8_t _tag : 1;
        std::uint8_t _data : 7;

        explicit location(register_ reg) : _tag(1), _data(std::uint8_t(reg)) {}
        explicit location(vstack) : _tag(0), _data(0) {}

        bool is_in_register() const
        {
            return _tag == 1;
        }

        register_ get_register() const
        {
            assert(is_in_register());
            return register_(_data);
        }
    };

public:
    void reset()
    {
        _value_stack.clear();
        for (auto r = 19; r <= 29; ++r)
            _unused_registers.push_back(lauf::aarch64::register_(r));
        _vstack_delta = 0;
    }

    void save_restore_registers(bool save, emitter& e, std::size_t max_vstack_size)
    {
        // Save callee saved registers x19-x29 and link register x30, in case we might need them.
        // Order must match the order in which registers are used (i.e. reverse from the call
        // above).
        constexpr register_ regs[] = {register_(30), register_(29), register_(28), register_(27),
                                      register_(26), register_(25), register_(24), register_(23),
                                      register_(22), register_(21), register_(20), register_(19)};

        auto last_idx = max_vstack_size + 1;
        if (last_idx >= sizeof(regs) / sizeof(register_))
            last_idx = sizeof(regs) / sizeof(register_) - 1;

        for (auto i = 0u; i <= last_idx; i += 2)
            if (save)
                e.push_pair(regs[i], regs[i + 1]);
            else
                e.pop_pair(regs[last_idx + 1 - i - 1], regs[last_idx + 1 - i]);
    }

    void enter_function(std::size_t input_count)
    {
        assert(_value_stack.empty());
        // The inputs of the function are the previous outputs.
        pop_outputs(input_count);
    }
    void exit_function(emitter& e, register_ vstack_ptr, std::size_t output_count)
    {
        assert(_value_stack.size() == output_count);
        // The outputs are inputs for the next function.
        push_inputs(e, vstack_ptr, output_count);
    }

    void branch(emitter& e, register_ vstack_ptr)
    {
        // On a branch, we need to ensure everything is flushed to the value stack.
        // This makes it unnecessary to figure out consistent register assignments.
        for (auto idx = 0; idx != _value_stack.size(); ++idx)
        {
            auto l = _value_stack[idx];
            if (l.is_in_register())
            {
                e.str_imm(l.get_register(), vstack_ptr, vstack_offset(idx));
                free_register(l.get_register());
                _value_stack[idx] = location(vstack{});
            }
        }

        // We also need to ensure the vstack_ptr is at a fixed location.
        flush_vstack_ptr(e, vstack_ptr);
    }

    void push_inputs(emitter& e, register_ vstack_ptr, std::size_t input_count)
    {
        for (auto idx = _value_stack.size() - input_count; idx != _value_stack.size(); ++idx)
        {
            auto l = _value_stack[idx];
            if (l.is_in_register())
            {
                // Value is in register, write it to the vstack at the index.
                e.str_imm(l.get_register(), vstack_ptr, vstack_offset(idx));
                free_register(l.get_register());
            }
        }

        // Move the vstack_ptr to the correct top.
        flush_vstack_ptr(e, vstack_ptr);

        // Resize value doesn't matter, we're shrinking it.
        _value_stack.resize(_value_stack.size() - input_count, location(vstack{}));
    }
    void pop_outputs(std::size_t output_count, std::int32_t stack_change = 0)
    {
        for (auto i = 0u; i != output_count; ++i)
            _value_stack.emplace_back(vstack{});

        // Move the vstack_ptr to the base.
        _vstack_delta += stack_change;
        _vstack_delta += std::ptrdiff_t(_value_stack.size());
    }

    register_ push_as_register()
    {
        auto reg = allocate_register();
        _value_stack.emplace_back(reg);
        return reg;
    }

    register_ top_as_register(emitter& e, register_ vstack_ptr)
    {
        if (auto location = _value_stack.back(); location.is_in_register())
        {
            return location.get_register();
        }
        else
        {
            auto reg = allocate_register();
            e.ldr_imm(reg, vstack_ptr, vstack_offset(_value_stack.size() - 1));
            return reg;
        }
    }

    register_ pop_as_register(emitter& e, register_ vstack_ptr)
    {
        auto reg = top_as_register(e, vstack_ptr);
        _value_stack.pop_back();
        free_register(reg);
        return reg;
    }

    void discard(std::size_t n)
    {
        for (auto i = 0u; i != n; ++i)
        {
            auto location = _value_stack.back();
            _value_stack.pop_back();
            if (location.is_in_register())
                free_register(location.get_register());
        }
    }

    void pick(emitter& e, register_ vstack_ptr, std::size_t pick_idx)
    {
        auto idx = _value_stack.size() - pick_idx - 1;
        auto l   = _value_stack[idx];
        if (!l.is_in_register())
        {
            // To duplicate it, we need to move it into a register.
            auto reg = allocate_register();
            e.ldr_imm(reg, vstack_ptr, vstack_offset(idx));
            l = location(reg);
        }

        // The value is in a register, simply keep it twice.
        _value_stack.push_back(l);
    }

    void roll(emitter& e, register_ vstack_ptr, std::size_t roll_idx)
    {
        // First move everything above into registers for simplicty.
        for (auto idx = _value_stack.size() - roll_idx - 1; idx != _value_stack.size(); ++idx)
        {
            auto l = _value_stack[idx];
            if (!l.is_in_register())
            {
                // Value is in register, write it to the vstack at the index.
                auto reg = allocate_register();
                e.ldr_imm(reg, vstack_ptr, vstack_offset(idx));
                l = location(reg);
            }
        }

        // Then we can just rotate the register assignments.
        auto iter = std::prev(_value_stack.end(), std::ptrdiff_t(roll_idx) + 1);
        std::rotate(iter, iter + 1, _value_stack.end());
    }

private:
    std::ptrdiff_t vstack_offset(std::size_t idx)
    {
        return -std::ptrdiff_t(idx) - 1 + _vstack_delta;
    }

    // Moves the vstack_ptr to the top of the stack.
    void flush_vstack_ptr(emitter& e, register_ vstack_ptr)
    {
        auto total_delta = _vstack_delta - std::ptrdiff_t(_value_stack.size());
        if (total_delta > 0)
            e.add_imm(vstack_ptr, vstack_ptr, std::uint16_t(total_delta * sizeof(lauf_value)));
        else if (total_delta < 0)
            e.sub_imm(vstack_ptr, vstack_ptr, std::uint16_t(-total_delta * sizeof(lauf_value)));
        _vstack_delta = 0;
    }

    register_ allocate_register()
    {
        assert(!_unused_registers.empty()); // TODO
        auto r = _unused_registers.back();
        _unused_registers.pop_back();
        return r;
    }

    void free_register(register_ reg)
    {
        auto used = std::any_of(_value_stack.begin(), _value_stack.end(), [&](location l) {
            return l.is_in_register() && l.get_register() == reg;
        });
        if (!used)
            _unused_registers.push_back(reg);
    }

    std::vector<location>  _value_stack;
    std::vector<register_> _unused_registers;
    // Delta that needs to be added to bring the stack pointer to the base.
    std::ptrdiff_t _vstack_delta;
};
} // namespace lauf::aarch64

#endif // SRC_LAUF_AARCH64_REGISTER_ALLOCATOR_HPP_INCLUDED

