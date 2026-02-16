//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Common/CommonHeaders.h"

#include <cstdint>
#include <string_view>

namespace carrot::audio {
    enum class audio_bus_id : uint8_t
    {
        master,
        music,
        sfx,
        ui,

        unknown,
        count
    };

    struct audio_bus_t
    {
        bool muted{ false };
        bool soloed{ false };
        float gain{ 1.0f };
        float pan{ 0.f }; // subgroup pan
        float* buffer{ nullptr }; // interleaved
    };

    inline audio_bus_id audio_bus_id_from_string(std::string_view bus)
    {
        if (bus == "music") return audio_bus_id::music;
        if (bus == "sfx") return audio_bus_id::sfx;
        if (bus == "ui") return audio_bus_id::ui;

        return audio_bus_id::unknown;
    }
} // namespace carrot::audio
