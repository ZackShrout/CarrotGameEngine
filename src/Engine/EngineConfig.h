//
// Created by Zack Shrout on 2/10/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "RHI/RHI.h"

namespace carrot {
    namespace io {
        class virtual_file_system_t;
    }

    struct engine_graphics_config_t
    {
        rhi::graphics_api api;
        bool enable_debug_layers;
        bool present_sync_enabled;
    };

    struct engine_audio_config_t
    {
        uint32_t sample_rate;
        uint32_t block_size;
        uint32_t channels;
    };

    struct engine_config_t
    {
        engine_graphics_config_t graphics;
        engine_audio_config_t audio;
    };

    engine_config_t load_engine_config(io::virtual_file_system_t& vfs);
} // namespace carrot
