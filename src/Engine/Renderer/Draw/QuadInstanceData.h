//
// Created by Zack Shrout on 4/20/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <chlm/CarrotHLM.h>

namespace carrot::renderer {
    struct gpu_quad_instance_t
    {
        chlm::float4 quad_rect_px{ 0.f, 0.f, 1.f, 1.f };
        chlm::float4 uv_rect{ 0.f, 0.f, 1.f, 1.f };
        chlm::float4 color{ 1.f, 1.f, 1.f, 1.f };
        chlm::float4 draw_params{ 0.f, 0.f, 0.f, 0.f };
    };
} // namespace carrot::renderer
