//
// Created by Zack Shrout.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/CoreDefines.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace carrot::core::memory {
    class ring_t
    {
    public:
        struct allocation_t
        {
            size_t offset_bytes{ 0u };
            size_t size_bytes{ 0u };
            bool wrapped{ false };
        };

        explicit ring_t(size_t capacity_bytes) noexcept;

        [[nodiscard]] std::optional<allocation_t> allocate(size_t size_bytes,
                                                           size_t alignment = alignof(std::max_align_t)) noexcept;
        void reset() noexcept;

        [[nodiscard]] size_t capacity() const noexcept { return _capacity; }
        [[nodiscard]] size_t cursor() const noexcept { return _cursor; }
        [[nodiscard]] bool empty() const noexcept { return _empty; }

    private:
        [[nodiscard]] static size_t align_up(size_t value, size_t alignment) noexcept;

        size_t _capacity{ 0u };
        size_t _cursor{ 0u };
        bool _empty{ true };
    };
} // namespace carrot::core::memory
