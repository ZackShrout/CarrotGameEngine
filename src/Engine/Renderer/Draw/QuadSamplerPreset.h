//
// Created by Zack Shrout on 3/27/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>

namespace carrot::renderer {
    enum class quad_sampler_preset_t : std::uint8_t
    {
        pixel_clamp, // nearest + clamp
        smooth_clamp, // linear  + clamp
        pixel_repeat, // nearest + repeat
        smooth_repeat, // linear  + repeat
    };
} // namespace carrot::renderer
