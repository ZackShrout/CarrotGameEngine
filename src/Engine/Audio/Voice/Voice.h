//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Envelope.h"

namespace carrot::audio {
    enum class voice_state : uint8_t
    {
        idle,       // free slot
        active,     // playing normally
        releasing,  // stolen / note-off, fading out
    };

    /**
     * @brief Single active audio voice.
     *
     * Lives entirely on the audio thread.
     */
    struct voice_t
    {
        voice_state state{ voice_state::idle };

        double phase{ 0.0 };
        double frequency{ 440.0 };
        double phase_inc{ 0.0 };
        float  gain{ 0.2f };

        uint64_t start_frame{ 0 };

        envelope_t envelope;
    };
} // namespace carrot::audio