//
// Created by zshrout on 3/14/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "QuadVertex.h"

#include <array>
#include <cstdint>

namespace carrot::renderer {
    inline constexpr std::array<quad_vertex_t, 4> k_unit_quad_vertices
    {
        {
            // top-left
            { .x = 0.0f, .y = 0.0f, .u = 0.0f, .v = 0.0f, .color = 0xFFFFFFFFu },
            // top-right
            { .x = 1.0f, .y = 0.0f, .u = 1.0f, .v = 0.0f, .color = 0xFFFFFFFFu },
            // bottom-right
            { .x = 1.0f, .y = 1.0f, .u = 1.0f, .v = 1.0f, .color = 0xFFFFFFFFu },
            // bottom-left
            { .x = 0.0f, .y = 1.0f, .u = 0.0f, .v = 1.0f, .color = 0xFFFFFFFFu },
        }
    };

    inline constexpr std::array<uint16_t, 6> k_unit_quad_indices
    {
        {
            0, 1, 2,
            0, 2, 3
        }
    };
} // namespace carrot::renderer
