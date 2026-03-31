//
// Created by Zack Shrout on 3/31/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <chlm/CarrotHLM.h>
#include <string>

namespace carrot::assets {
    struct sprite_frame_t
    {
        std::string name;
        chlm::uint_rect pixel_rect{ };
        chlm::float2 pivot{ 0.5f, 0.5f };
    };
} // namespace carrot::assets
