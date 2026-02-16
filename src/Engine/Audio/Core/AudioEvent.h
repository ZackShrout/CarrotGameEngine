//
// Created by Zack Shrout on 2/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Audio/Voice/VoiceHandle.h"

namespace carrot::audio {
    enum class audio_event_type : uint8_t
    {
        voice_finished,
    };

    struct audio_event_t
    {
        audio_event_type type{ audio_event_type::voice_finished };
        voice_handle_t handle;
    };
} // namespace carrot::audio