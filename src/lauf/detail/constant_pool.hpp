// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_DETAIL_CONSTANT_POOL_HPP_INCLUDED
#define SRC_DETAIL_CONSTANT_POOL_HPP_INCLUDED

#include <cstring>
#include <lauf/detail/bytecode.hpp>
#include <lauf/value.h>
#include <vector>

namespace lauf::_detail
{
class constant_pool
{
public:
    void reset() &&
    {
        _constants.clear();
    }

    bc_constant_idx insert(lauf_value value)
    {
        for (auto idx = std::size_t(0); idx != _constants.size(); ++idx)
            if (std::memcmp(&_constants[idx], &value, sizeof(lauf_value)) == 0)
                return bc_constant_idx(idx);

        auto idx = _constants.size();
        _constants.push_back(value);
        return bc_constant_idx(idx);
    }

    auto insert(lauf_value_int value)
    {
        lauf_value v;
        v.as_int = value;
        return insert(v);
    }
    auto insert(lauf_value_native_ptr value)
    {
        lauf_value v;
        v.as_native_ptr = value;
        return insert(v);
    }

    std::size_t size() const
    {
        return _constants.size();
    }
    const lauf_value* data() const
    {
        return _constants.data();
    }

private:
    std::vector<lauf_value> _constants;
};
} // namespace lauf::_detail

#endif // SRC_DETAIL_CONSTANT_POOL_HPP_INCLUDED

