//
// Created by Zack Shrout on 4/1/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <chlm/CarrotHLM.h>

namespace carrot::world {
    struct transform_component_t
    {
        chlm::float2 position{ 0.f, 0.f };
        chlm::float2 scale{ 1.f, 1.f };
        float rotation{ 0.f };
    };
} // namespace carrot::world
