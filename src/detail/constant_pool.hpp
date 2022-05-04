// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_DETAIL_CONSTANT_POOL_HPP_INCLUDED
#define SRC_DETAIL_CONSTANT_POOL_HPP_INCLUDED

#include <cstring>
#include <lauf/value.h>
#include <vector>

namespace lauf
{
class constant_pool
{
public:
    void reset() &&
    {
        _constants.clear();
    }

    std::size_t insert(lauf_Value value)
    {
        for (auto idx = std::size_t(0); idx != _constants.size(); ++idx)
            if (std::memcmp(&_constants[idx], &value, sizeof(lauf_Value)) == 0)
                return idx;

        auto idx = _constants.size();
        _constants.push_back(value);
        return idx;
    }

    std::size_t insert(lauf_ValueInt value)
    {
        lauf_Value v;
        v.as_int = value;
        return insert(v);
    }
    std::size_t insert(lauf_ValuePtr value)
    {
        lauf_Value v;
        v.as_ptr = value;
        return insert(v);
    }

    std::size_t size() const
    {
        return _constants.size();
    }
    const lauf_Value* data() const
    {
        return _constants.data();
    }

private:
    std::vector<lauf_Value> _constants;
};
} // namespace lauf

#endif // SRC_DETAIL_CONSTANT_POOL_HPP_INCLUDED

