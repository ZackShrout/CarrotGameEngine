//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "AudioEngine.h"

#include <cstring>

namespace carrot::audio {
    void audio_engine_t::init(audio_clock_t* clock, const uint32_t channels) noexcept
    {
        _clock = clock;
        _channels = channels;
    }

    void audio_engine_t::shutdown() noexcept
    {
        _clock = nullptr;
        _channels = 0;
    }

    void audio_engine_t::render(float* output, const uint32_t frame_count, const uint32_t channel_count) noexcept
    {
        (void)channel_count;

        const uint32_t sample_count{ frame_count * _channels };

        // Render silence
        std::memset(output, 0, sample_count * sizeof(float));

        // Advance audio time
        _clock->advance();
    }
} // namespace carrot::audio
