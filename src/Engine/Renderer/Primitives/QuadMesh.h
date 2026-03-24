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
            { .x = 0.f, .y = 0.f, .u = 0.f, .v = 0.f, .color = 0xFFFFFFFFu },
            // top-right
            { .x = 1.f, .y = 0.f, .u = 1.f, .v = 0.f, .color = 0xFFFFFFFFu },
            // bottom-right
            { .x = 1.f, .y = 1.f, .u = 1.f, .v = 1.f, .color = 0xFFFFFFFFu },
            // bottom-left
            { .x = 0.f, .y = 1.f, .u = 0.f, .v = 1.f, .color = 0xFFFFFFFFu },
        }
    };

    inline constexpr std::array<uint32_t, 6> k_unit_quad_indices
    {
        {
            0, 1, 2,
            0, 2, 3
        }
    };
} // namespace carrot::renderer
