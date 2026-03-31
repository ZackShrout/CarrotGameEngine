//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace carrot::assets {
    struct sprite_animation_t
    {
        std::string name;
        float fps{ 12.f };
        bool loop{ true };
        std::vector<uint32_t> frame_indices;
    };
} // namespace carrot::assets
