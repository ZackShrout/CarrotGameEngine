//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace carrot::assets {
    struct sprite_animation_frame_t
    {
        uint32_t frame_index{ 0 };
        float duration_seconds{ 0.1f };
    };

    struct sprite_animation_t
    {
        std::string name;
        bool loop{ true };
        std::vector<sprite_animation_frame_t> frames;
    };
} // namespace carrot::assets
