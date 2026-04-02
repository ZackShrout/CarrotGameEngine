//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Renderer/Draw/QuadSamplerPreset.h"
#include "Renderer/Draw/RenderLayer.h"

#include <chlm/CarrotHLM.h>
#include <cstdint>

namespace carrot::assets {
    class loaded_sprite_asset_t;
    struct sprite_frame_t;
}

namespace carrot::world {
    struct sprite_component_t
    {
        const assets::loaded_sprite_asset_t* sprite{ nullptr };
        const assets::sprite_frame_t* frame{ nullptr };

        bool use_size_override{ false };
        chlm::float2 size_override_world{ 1.f, 1.f };

        bool use_custom_pivot{ false };
        chlm::float2 pivot{ 0.5f, 0.5f };

        bool flip_x{ false };
        bool flip_y{ false };

        carrot::renderer::render_layer_t layer{ carrot::renderer::render_layer_t::actors };
        int32_t order_in_layer{ 0 };

        uint32_t color{ 0xFFFFFFFFu };
        carrot::renderer::quad_sampler_preset_t sampler_preset{
            carrot::renderer::quad_sampler_preset_t::pixel_clamp
        };
    };
} // namespace carrot::world
