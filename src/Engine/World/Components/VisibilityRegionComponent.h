//
// Created by Codex on 4/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <chlm/CarrotHLM.h>
#include <string>

namespace carrot::world {
    struct visibility_region_component_t
    {
        chlm::float2 size_world{ 0.f, 0.f };
        std::string tag;
    };
} // namespace carrot::world
