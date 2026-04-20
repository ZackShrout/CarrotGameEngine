//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace carrot::rhi {
    enum class buffer_usage_t : uint8_t
    {
        vertex,
        index,
        uniform,
        staging,
        shader_read,
        storage,
        indirect,
        readback
    };

    [[nodiscard]] constexpr std::string_view buffer_usage_to_string(const buffer_usage_t usage) noexcept
    {
        switch (usage)
        {
            case buffer_usage_t::vertex: return "vertex";
            case buffer_usage_t::index: return "index";
            case buffer_usage_t::uniform: return "uniform";
            case buffer_usage_t::staging: return "staging";
            case buffer_usage_t::shader_read: return "shader_read";
            case buffer_usage_t::storage: return "storage";
            case buffer_usage_t::indirect: return "indirect";
            case buffer_usage_t::readback: return "readback";
            default: return "unknown";
        }
    }

    [[nodiscard]] constexpr bool buffer_usage_prefers_upload_memory(const buffer_usage_t usage) noexcept
    {
        return usage == buffer_usage_t::staging;
    }

    [[nodiscard]] constexpr bool buffer_usage_prefers_readback_memory(const buffer_usage_t usage) noexcept
    {
        return usage == buffer_usage_t::readback;
    }

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
