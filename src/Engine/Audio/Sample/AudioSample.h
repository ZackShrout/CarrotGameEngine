//
// Created by Zack Shrout on 2/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>

namespace carrot::audio {
    /**
     * @brief Immutable decoded audio sample.
     *
     * Audio samples represent fully decoded PCM audio data
     * loaded on the game thread and referenced by audio voices
     * on the audio thread.
     *
     * @note AudioSample objects:
     *  - are read-only after creation
     *  - perform no allocation on the audio thread
     *  - may be referenced by multiple voices concurrently
     *  - must outlive any voice that uses them
     */
    struct audio_sample_t
    {
        const float* data{ nullptr }; // interleaved for now
        uint32_t frame_count{ };
        uint32_t channels{ };
        uint32_t sample_rate{ };
    };
} // namespace carrot::audio
