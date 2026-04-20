//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Renderer/Draw/ForwardPlusSharedConfig.h"

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

    struct forward_plus_frame_constants_t
    {
        chlm::float4 grid_params{ 0.f, 0.f, static_cast<float>(k_forward_plus_tile_size_px), 0.f };
        std::array<std::uint32_t, 4> tile_counts{ 0u, 0u, 0u, 0u };
        std::array<std::uint32_t, 4> point_light_counts{ 0u, 0u, 0u, 0u };
    };

    struct forward_plus_light_input_t
    {
        std::array<world_point_light_uniform_t, k_max_world_point_lights> point_lights{ };
    };

    struct forward_plus_classification_output_t
    {
        std::array<forward_plus_tile_header_t, k_max_forward_plus_tiles> tile_headers{ };
        std::array<packed_uint4_t, k_max_forward_plus_packed_light_index_words> packed_light_indices{ };
    };

    /**
     * @brief Per-frame world forward+ uniform payload for lit quad rendering.
     */
    struct world_forward_plus_uniform_t
    {
        chlm::float4x4 view_projection{ chlm::float4x4::identity() };
        chlm::float4 ambient_color{ 1.f, 1.f, 1.f, 1.f };
        std::array<std::uint32_t, 4> renderer_flags{ 0u, 0u, 0u, 0u };
        forward_plus_frame_constants_t forward_plus_constants{ };
        std::array<world_point_light_uniform_t, k_max_world_point_lights> point_lights{ };
        std::array<forward_plus_tile_header_t, k_max_forward_plus_tiles> forward_plus_tiles{ };
        std::array<packed_uint4_t, k_max_forward_plus_packed_light_index_words> forward_plus_light_indices{ };
    };

    [[nodiscard]] inline world_forward_plus_uniform_t pack_world_forward_plus_uniform(
        const chlm::float4x4& view_projection,
        const chlm::float4& ambient_color,
        const std::uint32_t world_draw_mode,
        const forward_plus_frame_constants_t& forward_plus_constants,
        const forward_plus_light_input_t& light_input,
        const forward_plus_classification_output_t& classification_output) noexcept
    {
        world_forward_plus_uniform_t packed{ };
        packed.view_projection = view_projection;
        packed.ambient_color = ambient_color;
        packed.renderer_flags = { world_draw_mode, 0u, 0u, 0u };
        packed.forward_plus_constants = forward_plus_constants;
        packed.point_lights = light_input.point_lights;
        packed.forward_plus_tiles = classification_output.tile_headers;
        packed.forward_plus_light_indices = classification_output.packed_light_indices;
        return packed;
    }

    [[nodiscard]] inline world_forward_plus_uniform_t pack_world_forward_plus_uniform(
        const chlm::float4x4& view_projection,
        const chlm::float4& ambient_color,
        const forward_plus_frame_constants_t& forward_plus_constants,
        const forward_plus_light_input_t& light_input,
        const forward_plus_classification_output_t& classification_output) noexcept
    {
        return pack_world_forward_plus_uniform(view_projection,
                                              ambient_color,
                                              0u,
                                              forward_plus_constants,
                                              light_input,
                                              classification_output);
    }
} // namespace carrot::renderer
