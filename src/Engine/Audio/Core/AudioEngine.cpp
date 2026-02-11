//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "AudioEngine.h"

#include <chlm/CarrotHLM.h>

#include <cstring>

namespace carrot::audio {
    namespace {
        constexpr bool   k_enable_test_tone{ true };
        constexpr double k_test_frequency{ 440.0 }; // A4
    } // anonymous namespace
    void audio_engine_t::init(audio_clock_t* clock, const uint32_t channels) noexcept
    {
        _clock = clock;
        _channels = channels;
        _phase = 0.0;
    }

    void audio_engine_t::shutdown() noexcept
    {
        _clock = nullptr;
        _channels = 0;
    }

    void audio_engine_t::render(float* output, const uint32_t frame_count, const uint32_t channel_count) noexcept
    {
        _clock->advance();

        if constexpr (!k_enable_test_tone)
        {
            const uint32_t total = frame_count * channel_count;
            for (uint32_t i = 0; i < total; ++i)
                output[i] = 0.0f;
            return;
        }

        const double sample_rate{ static_cast<double>(_clock->sample_rate()) };
        const double phase_inc{ chlm::pi_2 * k_test_frequency / sample_rate };
        uint32_t index{ 0 };

        for (uint32_t frame = 0; frame < frame_count; ++frame)
        {
            const float sample{ .2f * static_cast<float>(std::sin(_phase)) };

            _phase += phase_inc;
            if (_phase >= chlm::pi_2)
                _phase -= chlm::pi_2;

            for (uint32_t ch = 0; ch < channel_count; ++ch)
                output[index++] = sample;
        }
    }
} // namespace carrot::audio
