//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>

namespace carrot::renderer {
    enum class render_layer_t : std::uint16_t
    {
        background = 0,
        world_back = 100,
        actors = 200,
        world_front = 300,
        effects = 400,
        debug = 500,
        ui = 600
    };
} // namespace carrot::renderer
