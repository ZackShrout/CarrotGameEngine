//
// Created by zshrout on 3/14/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

namespace carrot::renderer {
    struct textured_quad_push_constants_t
    {
        float offset_x{ 0.0f };
        float offset_y{ 0.0f };
        float scale_x{ 1.0f };
        float scale_y{ 1.0f };

        float uv_min_x{ 0.0f };
        float uv_min_y{ 0.0f };
        float uv_max_x{ 1.0f };
        float uv_max_y{ 1.0f };

        float tint_r{ 1.0f };
        float tint_g{ 1.0f };
        float tint_b{ 1.0f };
        float tint_a{ 1.0f };
    };

    static_assert(sizeof(textured_quad_push_constants_t) == 48);
} // namespace carrot::renderer
