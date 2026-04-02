//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "GameView.h"

namespace carrot::assets {
    class asset_manager_t;
}

namespace carrot::world {
    class world_t;
}

namespace carrot::core {
    struct game_context_t
    {
        world::world_t& world;
        assets::asset_manager_t& assets;
        game_view_t& view;
    };
} // namespace carrot::core
