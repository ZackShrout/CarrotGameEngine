//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "QuadSamplerPreset.h"
#include "RenderLayer.h"

#include <chlm/CarrotHLM.h>
#include <cstdint>

namespace carrot::assets {
    struct sprite_frame_t;
    class loaded_sprite_asset_t;
}

namespace carrot::renderer {
    struct sprite_draw_info_t
    {
        const assets::loaded_sprite_asset_t* sprite{ nullptr };
        const assets::sprite_frame_t* frame{ nullptr };

        float x{ 0.f };
        float y{ 0.f };
        float width{ 1.f };
        float height{ 1.f };
        bool use_custom_pivot{ false };
        chlm::float2 pivot{ 0.5f, 0.5f };
        bool flip_x{ false };
        bool flip_y{ false };

        render_layer_t layer{ render_layer_t::actors };
        render_order_mode_t order_mode{ render_order_mode_t::explicit_order };
        int32_t order_in_layer{ 0 };
        float sort_reference_y{ 0.f };

        uint32_t color{ 0xFFFFFFFFu };
        quad_sampler_preset_t sampler_preset{ quad_sampler_preset_t::pixel_clamp };
    };
} // namespace carrot::renderer
