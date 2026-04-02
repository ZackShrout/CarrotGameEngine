//
// Created by Zack Shrout on 4/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <CarrotEngine.h>

namespace sandbox {
    inline constexpr std::string_view k_bootstrap_scene_id{ "scene.test.overworld" };

    bool bootstrap_scene(carrot::core::game_context_t& game,
                         std::string_view scene_id,
                         std::string_view spawn_marker_override = {});
} // namespace sandbox
