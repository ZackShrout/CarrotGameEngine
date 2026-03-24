//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstddef>
#include <cstdint>

namespace carrot::rhi {
    enum class buffer_usage_t : uint8_t
    {
        vertex,
        index,
        uniform,
        staging
    };

    struct buffer_create_info_t
    {
        size_t size_bytes{ 0 };
        buffer_usage_t usage{ buffer_usage_t::vertex };
        const void* initial_data{ nullptr };
        bool cpu_writable{ false };
    };

    class rhi_buffer_t
    {
    public:
        virtual ~rhi_buffer_t() = default;

        [[nodiscard]] size_t size_bytes() const noexcept { return _size_bytes; }
        [[nodiscard]] buffer_usage_t usage() const noexcept { return _usage; }
        [[nodiscard]] virtual bool write(const void* data, size_t size_bytes, size_t offset_bytes = 0) = 0;

    protected:
        rhi_buffer_t(const size_t size_bytes, const buffer_usage_t usage) noexcept
            : _size_bytes(size_bytes), _usage(usage) {}

    private:
        size_t _size_bytes{ 0 };
        buffer_usage_t _usage{ buffer_usage_t::vertex };
    };
} // namespace carrot::rhi
