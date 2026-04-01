//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <chlm/CarrotHLM.h>

namespace carrot::renderer {
    /**
     * @brief Per-frame camera uniform for textured quad rendering.
     */
    struct textured_quad_camera_uniform_t
    {
        chlm::float4x4 view_projection{ chlm::float4x4::identity() };
    };
} // namespace carrot::renderer