//
// Created by zshro on 3/21/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "RHI/Texture.h"

#include <cstdint>

namespace carrot::renderer {
    struct textured_quad_draw_info_t
    {
        const rhi::rhi_texture_t* texture{ nullptr };

        float x{ 0.f };
        float y{ 0.f };
        float width{ 1.f };
        float height{ 1.f };

        float u0{ 0.f };
        float v0{ 0.f };
        float u1{ 1.f };
        float v1{ 1.f };

        uint32_t color{ 0xFFFFFFFF }; // ABGR
    };
}
