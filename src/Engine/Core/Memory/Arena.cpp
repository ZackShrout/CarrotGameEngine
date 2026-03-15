//
// Created by Zack Shrout on 2/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Arena.h"

namespace carrot::core::memory {
    namespace {
        size_t align_up(size_t value, size_t alignment)
        {
            return value + alignment - 1 & ~(alignment - 1);
        }
    } // anonymous namespace

    arena_t::arena_t(const size_t capacity_bytes) : _capacity{ capacity_bytes }
    {
        _buffer = static_cast<uint8_t*>(std::malloc(_capacity));

        if (!_buffer)
        {
            LOG_CORE_FATAL("Failed to allocate {} bytes for arena buffer", _capacity);
        }
    }

    arena_t::~arena_t()
    {
        std::free(_buffer);
        _buffer = nullptr;
    }

    void* arena_t::allocate(const size_t size, const size_t alignment)
    {
        const size_t aligned_offset{ align_up(_offset, alignment) };
        const size_t next_offset{ aligned_offset + size };

        CE_ASSERT(next_offset <= _capacity && "Arena out of memory");

        void* ptr{ _buffer + aligned_offset };
        _offset = next_offset;

        return ptr;
    }

    void arena_t::reset()
    {
        _offset = 0;
    }
} // namespace carrot::core::memory
