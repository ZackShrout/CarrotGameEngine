//
// Created by Zack Shrout on 3/27/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "QuadSamplerPreset.h"
#include "RenderLayer.h"

#include <cstddef>
#include <cstdint>
#include <functional>

namespace carrot::rhi {
    class rhi_texture_t;
}

namespace carrot::renderer {

    struct uv_rect_t
    {
        float u_min{ 0.0f };
        float v_min{ 0.0f };
        float u_max{ 1.0f };
        float v_max{ 1.0f };

        [[nodiscard]] bool operator==(const uv_rect_t& other) const noexcept = default;
    };

    struct textured_quad_draw_info_t
    {
        const rhi::rhi_texture_t* texture{ nullptr };

        float x{ 0.f };
        float y{ 0.f };
        float width{ 1.f };
        float height{ 1.f };

        float u0{ 0.f };
        float v0{ 0.f };
        float u1{ 1.f };
        float v1{ 1.f };

        render_layer_t layer{ render_layer_t::world_back };
        int32_t order_in_layer{ 0 };

        uint32_t color{ 0xFFFFFFFF }; // ABGR

        quad_sampler_preset_t sampler_preset{ quad_sampler_preset_t::smooth_clamp };
    };

    struct textured_quad_batch_key_t
    {
        rhi::rhi_texture_t* texture{ nullptr };
        quad_sampler_preset_t sampler_preset{ quad_sampler_preset_t::smooth_clamp };

        [[nodiscard]] bool operator==(const textured_quad_batch_key_t& other) const noexcept = default;
    };

    struct textured_quad_batch_key_hash_t
    {
        [[nodiscard]] std::size_t operator()(const textured_quad_batch_key_t& key) const noexcept
        {
            const std::size_t h1{ std::hash<rhi::rhi_texture_t*>{}(key.texture) };
            const std::size_t h2{ std::hash<std::uint8_t>{}(static_cast<std::uint8_t>(key.sampler_preset)) };

            return h1 ^ (h2 + 0x9e3779b9u + (h1 << 6u) + (h1 >> 2u));
        }
    };

} // namespace carrot::renderer
