// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_SUPPORT_JOINED_ALLOCATION_HPP_INCLUDED
#define SRC_LAUF_SUPPORT_JOINED_ALLOCATION_HPP_INCLUDED

#include <lauf/config.h>
#include <lauf/support/stack_allocator.hpp>
#include <utility>

namespace lauf
{
template <typename T, typename... List>
struct _joined_offsets;
template <typename T, typename Head, typename... Tail>
struct _joined_offsets<T, Head, Tail...>
{
    using tail = _joined_offsets<T, Tail...>;

    static constexpr std::size_t index = 1 + tail::index;

    static constexpr std::size_t get_align(stack_allocator_offset allocator)
    {
        // We allocate zero objects to get the alignment offset only.
        allocator.allocate<Head>(0);
        return tail::get_align(allocator);
    }

    template <typename... SizeT>
    static constexpr std::size_t get_size(std::size_t h, SizeT... t)
    {
        return h * sizeof(Head) + tail::get_size(t...);
    }
};
template <typename T, typename... Tail>
struct _joined_offsets<T, T, Tail...>
{
    static constexpr std::size_t index = 0;

    static constexpr std::size_t get_align(stack_allocator_offset allocator)
    {
        return allocator.size();
    }

    static constexpr std::size_t get_size()
    {
        return 0;
    }
};

template <typename HeaderType, typename... ArrayTypes>
class joined_allocation
{
    static_assert((std::is_trivially_copyable_v<ArrayTypes> && ...));

public:
    //=== construction ===//
    template <typename... SizeT>
    static HeaderType* create(SizeT... sizes)
    {
        static_assert(sizeof...(sizes) == sizeof...(ArrayTypes));

        stack_allocator_offset allocator;
        allocator.allocate<HeaderType>();
        (allocator.allocate<ArrayTypes>(sizes), ...);

        auto memory = ::operator new(allocator.size());
        return ::new (memory) HeaderType{};
    }

    static void destroy(HeaderType* ptr)
    {
        ptr->~HeaderType();
        ::operator delete(ptr);
    }

    joined_allocation(const joined_allocation&) = delete;
    joined_allocation& operator=(const joined_allocation&) = delete;

    ~joined_allocation() = default;

    //=== array access ===//
    template <typename T, typename... SizeT>
    T* array(SizeT... previous_sizes) noexcept
    {
        const auto& cthis = *this;
        return const_cast<T*>(cthis.template array<T>(previous_sizes...));
    }
    template <typename T, typename... SizeT>
    const T* array(SizeT... previous_sizes) const noexcept
    {
        using offsets = _joined_offsets<T, HeaderType, ArrayTypes...>;
        static_assert(sizeof...(previous_sizes) == offsets::index - 1);

        constexpr auto align_offsets = offsets::get_align(stack_allocator_offset());

        auto size_offsets = offsets::get_size(1, previous_sizes...);
        return reinterpret_cast<const T*>(reinterpret_cast<const unsigned char*>(this)
                                          + align_offsets + size_offsets);
    }

protected:
    joined_allocation() = default;
};
} // namespace lauf

#endif // SRC_LAUF_SUPPORT_JOINED_ALLOCATION_HPP_INCLUDED

