//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstddef>
#include <cstdint>

namespace carrot::rhi {
    enum texture_usage_bits_t : std::uint32_t
    {
        texture_usage_sampled = 1u << 0u,
        texture_usage_transfer_src = 1u << 1u,
        texture_usage_transfer_dst = 1u << 2u,
        texture_usage_color_attachment = 1u << 3u
    };

    [[nodiscard]] constexpr bool texture_usage_includes(const std::uint32_t usage_mask,
                                                        const texture_usage_bits_t usage) noexcept
    {
        return (usage_mask & static_cast<std::uint32_t>(usage)) != 0u;
    }

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
        std::uint32_t usage_mask{ texture_usage_sampled | texture_usage_transfer_dst };

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
        [[nodiscard]] virtual bool has_initial_data() const noexcept { return false; }
    };

    struct render_target_create_info_t
    {
        uint32_t width{ 0 };
        uint32_t height{ 0 };
        texture_format_t format{ texture_format_t::rgba8_srgb };
        bool shader_readable{ true };
    };

    class rhi_render_target_t
    {
    public:
        virtual ~rhi_render_target_t() = default;

        [[nodiscard]] virtual uint32_t width() const noexcept = 0;
        [[nodiscard]] virtual uint32_t height() const noexcept = 0;
        [[nodiscard]] virtual texture_format_t format() const noexcept = 0;
        [[nodiscard]] virtual rhi_texture_t* color_texture() const noexcept = 0;
    };
} // carrot::rhi
