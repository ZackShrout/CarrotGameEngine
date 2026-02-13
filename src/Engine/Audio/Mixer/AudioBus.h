//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>

namespace carrot::audio {
    struct audio_bus_t
    {
        bool muted{ false };
        bool soloed{ false };
        float gain{ 1.0f };
        float pan{ 0.f }; // subgroup pan
        float* buffer{ nullptr }; // interleaved
    };
} // namespace carrot::audio