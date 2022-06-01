// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#ifndef SRC_LAUF_SUPPORT_JOINED_ALLOCATION_HPP_INCLUDED
#define SRC_LAUF_SUPPORT_JOINED_ALLOCATION_HPP_INCLUDED

#include <cassert>
#include <initializer_list>
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
};
template <typename T, typename... Tail>
struct _joined_offsets<T, T, Tail...>
{
    static constexpr std::size_t index = 0;

    static constexpr std::size_t get_align(stack_allocator_offset allocator)
    {
        return allocator.size();
    }
};

template <typename HeaderType, typename... ArrayTypes>
class joined_allocation
{
public:
    //=== construction ===//
    static void* allocate(std::initializer_list<std::size_t> sizes)
    {
        assert(sizes.size() == sizeof...(ArrayTypes));

        stack_allocator_offset allocator;
        allocator.allocate<HeaderType>();

        auto ptr = sizes.begin();
        (allocator.allocate<ArrayTypes>(*ptr++), ...);

        return ::operator new(allocator.size());
    }
    static void* allocate(std::size_t size)
    {
        return allocate({size});
    }

    template <typename... Args>
    static HeaderType* create(std::initializer_list<std::size_t> sizes, Args&&... args)
    {
        auto memory = allocate(sizes);
        return ::new (memory) HeaderType(std::forward<Args>(args)...);
    }
    template <typename... Args>
    static HeaderType* create(std::size_t size, Args&&... args)
    {
        auto memory = allocate({size});
        return ::new (memory) HeaderType(std::forward<Args>(args)...);
    }

    static void resize(HeaderType*& ptr, std::initializer_list<std::size_t> cur_sizes,
                       std::initializer_list<std::size_t> new_sizes)
    {
        static_assert((std::is_trivially_copyable_v<ArrayTypes> && ...
                       && std::is_trivially_copyable_v<HeaderType>));

        auto new_memory = allocate(new_sizes);
        std::memcpy(new_memory, ptr, sizeof(HeaderType));
        auto new_ptr = static_cast<HeaderType*>(new_memory);

        auto cur_size_ptr = cur_sizes.begin();
        (std::memcpy(new_ptr->template array<ArrayTypes>(new_sizes),
                     ptr->template array<ArrayTypes>(cur_sizes),
                     *cur_size_ptr++ * sizeof(ArrayTypes)),
         ...);

        destroy(ptr);
        ptr = new_ptr;
    }

    static void destroy(HeaderType* ptr)
    {
        ptr->~HeaderType();
        ::operator delete(ptr);
    }

protected:
    joined_allocation() = default;

public:
    joined_allocation(const joined_allocation&) = delete;
    joined_allocation& operator=(const joined_allocation&) = delete;

    ~joined_allocation() = default;

    //=== array access ===//
    template <typename T>
    T* array(std::initializer_list<std::size_t> previous_sizes) noexcept
    {
        const auto& cthis = *this;
        return const_cast<T*>(cthis.template array<T>(previous_sizes));
    }
    template <typename T>
    const T* array(std::initializer_list<std::size_t> previous_sizes) const noexcept
    {
        using offsets = _joined_offsets<T, HeaderType, ArrayTypes...>;
        assert(previous_sizes.size() >= offsets::index - 1);

        constexpr auto align_offset = offsets::get_align(stack_allocator_offset());

        auto size_offset = sizeof(HeaderType);
        if constexpr (offsets::index > 1)
        {
            constexpr std::size_t sizeofs[] = {sizeof(ArrayTypes)...};

            for (auto i = 0u; i != offsets::index - 1; ++i)
                size_offset += sizeofs[i] * previous_sizes.begin()[i];
        }

        return reinterpret_cast<const T*>(reinterpret_cast<const unsigned char*>(this)
                                          + align_offset + size_offset);
    }
};
} // namespace lauf

#endif // SRC_LAUF_SUPPORT_JOINED_ALLOCATION_HPP_INCLUDED

