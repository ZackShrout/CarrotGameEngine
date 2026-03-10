//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "AudioBus.h"

namespace carrot::audio {

    /**
     * @brief Per-voice routing parameters.
     *
     * Describes where a voice's dry signal goes and how much of it is sent
     * to shared effect buses (e.g. reverb).
     *
     * This struct lives primarily on the engine thread and is translated
     * into bus state and send levels on the audio thread via commands.
     */
    struct voice_routing_t
    {
        /** Primary output bus for the voice's dry signal. */
        audio_bus_id bus{ audio_bus_id::sfx };

        /** Reverb send level (0.0 = no send, 1.0 = full). */
        float reverb_send{ 0.f };

        // Future: additional sends (delay, chorus, etc.)
        // float delay_send{ 0.f };
    };

} // namespace carrot::audio