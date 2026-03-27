//
// Created by Zack Shrout on 3/27/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>

namespace carrot::renderer {
    enum class sampler_filter_t : uint8_t
    {
        nearest,
        linear,
    };

    enum class sampler_wrap_t : uint8_t
    {
        clamp,
        repeat,
    };
} // namespace carrot::renderer