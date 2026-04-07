//
// Created by zshrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <string_view>

namespace carrot::assets {
    class asset_manager_t;
}

namespace carrot::core {
    struct game_context_t;
}

namespace carrot::world {
    class world_t;

    class scene_loader_t
    {
    public:
        [[nodiscard]] static bool load_scene(core::game_context_t& game,
                                            std::string_view scene_id,
                                            std::string_view spawn_marker_override = {});
        [[nodiscard]] static bool load_scene(world_t& world,
                                            assets::asset_manager_t& assets,
                                            std::string_view scene_id,
                                            std::string_view spawn_marker_override = {});
    };
} // namespace carrot::world
