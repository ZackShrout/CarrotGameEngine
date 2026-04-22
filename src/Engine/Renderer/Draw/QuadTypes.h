//
// Created by Zack Shrout on 4/20/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "TexturedQuadBatch.h"
#include "TexturedQuadTypes.h"

#include <chlm/CarrotHLM.h>

#include <cstdint>
#include <vector>

namespace carrot::renderer {
    enum class frame_stage_kind_t : std::uint8_t
    {
        world = 0,
        ui,
        composite,
        overlay_debug,
        log_console,
        count
    };

    enum class frame_stage_space_t : std::uint8_t
    {
        world_camera = 0,
        viewport_pixels,
        render_target_pixels
    };

    enum class quad_content_kind_t : std::uint8_t
    {
        textured = 0,
        text
    };

    struct quad_target_id_t
    {
        std::uint16_t target{ 0u };
        std::uint16_t pass{ 0u };

        [[nodiscard]] bool operator==(const quad_target_id_t& other) const noexcept = default;
    };

    struct quad_instance_t
    {
        frame_stage_kind_t stage{ frame_stage_kind_t::world };
        frame_stage_space_t stage_space{ frame_stage_space_t::world_camera };
        quad_target_id_t target_id{ };
        quad_content_kind_t content_kind{ quad_content_kind_t::textured };
        const rhi::rhi_texture_t* texture{ nullptr };
        quad_sampler_preset_t sampler_preset{ quad_sampler_preset_t::smooth_clamp };
        world_material_key_t world_material{ };
        float x{ 0.f };
        float y{ 0.f };
        float width{ 1.f };
        float height{ 1.f };
        uv_rect_t uv_rect{ };
        render_layer_t layer{ render_layer_t::world_back };
        render_order_mode_t order_mode{ render_order_mode_t::explicit_order };
        int32_t order_in_layer{ 0 };
        float sort_reference_y{ 0.f };
        std::uint32_t color{ 0xFFFFFFFFu };
        float effect_mode{ 0.f };
        float effect_param0{ 0.f };
        chlm::float2 bounds_min_px{ 0.f, 0.f };
        chlm::float2 bounds_max_px{ 1.f, 1.f };
        std::uint32_t presentation_mask{ 1u };
        std::uint64_t submission_index{ 0u };
    };

    struct quad_bucket_key_t
    {
        frame_stage_kind_t stage{ frame_stage_kind_t::world };
        quad_target_id_t target_id{ };
        quad_content_kind_t content_kind{ quad_content_kind_t::textured };
        const rhi::rhi_texture_t* texture{ nullptr };
        quad_sampler_preset_t sampler_preset{ quad_sampler_preset_t::smooth_clamp };
        world_material_key_t world_material{ };

        [[nodiscard]] bool operator==(const quad_bucket_key_t& other) const noexcept = default;
    };

    struct quad_stream_t
    {
        std::vector<quad_instance_t> instances;
    };
} // namespace carrot::renderer
