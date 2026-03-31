//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "QuadSamplerPreset.h"

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

        uint32_t color{ 0xFFFFFFFFu };
        quad_sampler_preset_t sampler_preset{ quad_sampler_preset_t::pixel_clamp };
    };
} // namespace carrot::renderer
