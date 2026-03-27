//
// Created by Zack Shrout on 3/23/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "RHI/Texture.h"

#include <cstdint>

namespace carrot::renderer {
    struct textured_quad_batch_t
    {
        const rhi::rhi_texture_t* texture{ nullptr };
        uint32_t first_index{ 0 };
        uint32_t index_count{ 0 };
    };
} // namespace carrot::renderer
