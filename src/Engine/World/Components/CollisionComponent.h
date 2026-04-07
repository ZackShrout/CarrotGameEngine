//
// Created by zshrout on 4/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <chlm/CarrotHLM.h>

#include <cstdint>
#include <optional>

namespace carrot::world {
    struct collision_debug_display_t
    {
        bool filled{ false };
        float outline_thickness{ 2.f };
        uint32_t color{ 0xFF00FF00u };
    };

    struct collision_component_t
    {
        chlm::float2 half_extents{ 0.3f, 0.2f };
        chlm::float2 offset{ 0.f, -0.2f };
        std::optional<collision_debug_display_t> debug_display;
    };
} // namespace carrot::world
