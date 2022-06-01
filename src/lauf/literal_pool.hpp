// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LITERAL_POOL_HPP_INCLUDED
#define SRC_LITERAL_POOL_HPP_INCLUDED

#include <cstring>
#include <lauf/bytecode.hpp>
#include <lauf/value.h>
#include <vector>

namespace lauf
{
class literal_pool
{
public:
    void reset()
    {
        _literals.clear();
    }

    bc_literal_idx insert(lauf_value value)
    {
        for (auto idx = std::size_t(0); idx != _literals.size(); ++idx)
            if (std::memcmp(&_literals[idx], &value, sizeof(lauf_value)) == 0)
                return bc_literal_idx(idx);

        auto idx = _literals.size();
        _literals.push_back(value);
        return bc_literal_idx(idx);
    }

    auto insert(lauf_value_sint value)
    {
        lauf_value v;
        v.as_sint = value;
        return insert(v);
    }
    auto insert(lauf_value_address value)
    {
        lauf_value v;
        v.as_address = value;
        return insert(v);
    }
    auto insert(lauf_value_native_ptr value)
    {
        lauf_value v;
        v.as_native_ptr = value;
        return insert(v);
    }

    bool empty() const
    {
        return _literals.empty();
    }
    std::size_t size() const
    {
        return _literals.size();
    }
    const lauf_value* data() const
    {
        return _literals.data();
    }

private:
    std::vector<lauf_value> _literals;
};
} // namespace lauf

#endif // SRC_LITERAL_POOL_HPP_INCLUDED

