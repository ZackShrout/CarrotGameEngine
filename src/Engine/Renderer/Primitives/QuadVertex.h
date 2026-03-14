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
    };

    static_assert(sizeof(quad_vertex_t) == 20);
} // namespace carrot::renderer
