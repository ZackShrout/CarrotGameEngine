//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Renderer/Draw/QuadSamplerPreset.h"
#include "Renderer/Draw/RenderLayer.h"

#include <chlm/CarrotHLM.h>
#include <cstdint>

namespace carrot::assets {
    class loaded_tilemap_asset_t;
}

namespace carrot::world {
    struct tile_object_component_t
    {
        const assets::loaded_tilemap_asset_t* tilemap{ nullptr };
        uint32_t gid{ 0 };
        chlm::float2 size_source_px{ 0.f, 0.f };
        renderer::render_layer_t layer{ renderer::render_layer_t::actors };
        renderer::render_order_mode_t order_mode{ renderer::render_order_mode_t::explicit_order };
        int32_t order_in_layer{ 0 };
        float sort_reference_y{ 0.f };
        renderer::quad_sampler_preset_t sampler_preset{ renderer::quad_sampler_preset_t::pixel_clamp };
        uint32_t color{ 0xFFFFFFFFu };
    };
} // namespace carrot::world
