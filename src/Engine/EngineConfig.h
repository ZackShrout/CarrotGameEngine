//
// Created by Zack Shrout on 2/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "RHI/RHI.h"

namespace carrot {

    struct engine_graphics_config_t
    {
        rhi::graphics_api api;
        bool enable_debug_layers;
    };

    struct engine_config_t
    {
        engine_graphics_config_t graphics;
    };

    engine_config_t load_engine_config();
} // namespace carrot
