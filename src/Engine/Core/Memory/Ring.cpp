//
// Created by Zack Shrout.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Ring.h"

namespace carrot::core::memory {
    ring_t::ring_t(const size_t capacity_bytes) noexcept
        : _capacity{ capacity_bytes }
    {
    }

    std::optional<ring_t::allocation_t> ring_t::allocate(const size_t size_bytes, const size_t alignment) noexcept
    {
        if (size_bytes == 0u || _capacity == 0u)
            return std::nullopt;

        if (size_bytes > _capacity)
            return std::nullopt;

        const size_t safe_alignment{ std::max<size_t>(alignment, 1u) };
        const size_t aligned_cursor{ align_up(_cursor, safe_alignment) };
        if (aligned_cursor + size_bytes <= _capacity)
        {
            _cursor = aligned_cursor + size_bytes;
            _empty = false;
            return allocation_t{
                .offset_bytes = aligned_cursor,
                .size_bytes = size_bytes,
                .wrapped = false
            };
        }

        const size_t wrapped_offset{ align_up(0u, safe_alignment) };
        if (wrapped_offset + size_bytes > _capacity)
            return std::nullopt;

        _cursor = wrapped_offset + size_bytes;
        _empty = false;
        return allocation_t{
            .offset_bytes = wrapped_offset,
            .size_bytes = size_bytes,
            .wrapped = true
        };
    }

    void ring_t::reset() noexcept
    {
        _cursor = 0u;
        _empty = true;
    }

    size_t ring_t::align_up(const size_t value, const size_t alignment) noexcept
    {
        return (value + alignment - 1u) & ~(alignment - 1u);
    }
} // namespace carrot::core::memory
