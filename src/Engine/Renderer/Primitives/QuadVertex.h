//
// Created by zshrout on 3/14/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>

namespace carrot::renderer {
    struct quad_vertex_t
    {
        float x{ 0.f };
        float y{ 0.f };
        float u{ 0.f };
        float v{ 0.f };
        uint32_t color{ 0xFFFFFFFF }; // ABGR
        float effect_mode{ 0.f };
        float effect_param0{ 0.f };
    };

    static_assert(sizeof(quad_vertex_t) == 28);
} // namespace carrot::renderer
