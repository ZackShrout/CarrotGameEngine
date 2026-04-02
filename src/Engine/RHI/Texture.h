//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstddef>
#include <cstdint>

namespace carrot::rhi {
    enum class texture_format_t : uint8_t
    {
        rgba8_unorm,
        rgba8_srgb,
    };

    struct texture_create_info_t
    {
        uint32_t width{ 0 };
        uint32_t height{ 0 };
        texture_format_t format{ texture_format_t::rgba8_srgb };

        const void* initial_data{ nullptr };
        size_t initial_data_size{ 0 };
        uint32_t initial_data_stride_bytes{ 0 };
    };

    class rhi_texture_t
    {
    public:
        virtual ~rhi_texture_t() = default;

        [[nodiscard]] virtual uint32_t width() const noexcept = 0;
        [[nodiscard]] virtual uint32_t height() const noexcept = 0;
        [[nodiscard]] virtual texture_format_t format() const noexcept = 0;
    };
} // carrot::rhi
