//
// Created by zshrout on 4/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <string>

namespace carrot::world {
    struct trigger_component_t
    {
        std::string trigger_id;
        std::string trigger_kind;
    };
} // namespace carrot::world
