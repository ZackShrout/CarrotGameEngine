//
// Created by Zack Shrout on 2/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/CoreDefines.h"

#include <cstdint>
#include <utility>

namespace carrot::core::memory {
    class arena_t
    {
    public:
        explicit arena_t(size_t capacity_bytes);
        ~arena_t();

        DISABLE_COPY(arena_t)

        void* allocate(size_t size, size_t alignment = alignof(std::max_align_t));

        template<typename T, typename... Args>
        T* emplace(Args&&... args)
        {
            void* mem{ allocate(sizeof(T), alignof(T)) };
            return new(mem) T(std::forward<Args>(args)...);
        }

        void reset();

        [[nodiscard]] size_t capacity() const { return _capacity; }
        [[nodiscard]] size_t used() const { return _offset; }
        [[nodiscard]] size_t remaining() const { return _capacity - _offset; }

    private:
        uint8_t* _buffer{ nullptr };
        size_t _capacity{ 0 };
        size_t _offset{ 0 };
    };
} // namespace carrot::core::memory
