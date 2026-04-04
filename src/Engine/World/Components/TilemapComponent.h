//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Renderer/Draw/QuadSamplerPreset.h"
#include "Renderer/Draw/RenderLayer.h"

#include <cstdint>

namespace carrot::assets {
    class loaded_tilemap_asset_t;
}

namespace carrot::world {
    struct tilemap_component_t
    {
        const assets::loaded_tilemap_asset_t* tilemap{ nullptr };
        bool include_object_layers{ false };
        renderer::render_layer_t layer{ renderer::render_layer_t::world_back };
        renderer::render_order_mode_t order_mode{ renderer::render_order_mode_t::explicit_order };
        int32_t order_in_layer{ 0 };
        float sort_reference_y{ 0.f };
        renderer::quad_sampler_preset_t sampler_preset{ renderer::quad_sampler_preset_t::pixel_clamp };
        uint32_t color{ 0xFFFFFFFFu };
    };
} // namespace carrot::world
