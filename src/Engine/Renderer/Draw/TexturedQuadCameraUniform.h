//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "ForwardPlusSharedConfig.h"

#include <array>
#include <cstdint>

#include <chlm/CarrotHLM.h>

namespace carrot::renderer {
    struct world_point_light_uniform_t
    {
        chlm::float4 position_radius_px{ 0.f, 0.f, 0.f, 0.f };
        chlm::float4 color_intensity{ 1.f, 1.f, 1.f, 1.f };
    };

    struct forward_plus_tile_header_t
    {
        std::uint32_t light_index_offset{ 0u };
        std::uint32_t light_count{ 0u };
        std::uint32_t reserved0{ 0u };
        std::uint32_t reserved1{ 0u };
    };

    struct packed_uint4_t
    {
        std::uint32_t x{ 0u };
        std::uint32_t y{ 0u };
        std::uint32_t z{ 0u };
        std::uint32_t w{ 0u };
    };

    /**
     * @brief Per-frame world forward+ uniform payload for lit quad rendering.
     */
    struct world_forward_plus_uniform_t
    {
        chlm::float4x4 view_projection{ chlm::float4x4::identity() };
        chlm::float4 ambient_color{ 1.f, 1.f, 1.f, 1.f };
        chlm::float4 forward_plus_grid_params{ 0.f, 0.f, static_cast<float>(k_forward_plus_tile_size_px), 0.f };
        std::array<std::uint32_t, 4> forward_plus_tile_counts{ 0u, 0u, 0u, 0u };
        std::array<std::uint32_t, 4> point_light_counts{ 0u, 0u, 0u, 0u };
        std::array<world_point_light_uniform_t, k_max_world_point_lights> point_lights{ };
        std::array<forward_plus_tile_header_t, k_max_forward_plus_tiles> forward_plus_tiles{ };
        std::array<packed_uint4_t, k_max_forward_plus_packed_light_index_words> forward_plus_light_indices{ };
    };
} // namespace carrot::renderer
