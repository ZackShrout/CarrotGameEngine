//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "AudioEngine.h"

#include <chlm/CarrotHLM.h>

#include <cstring>

#define CARROT_TEST_TONE 0
#define CARROT_TEST_SWEEP 1

namespace carrot::audio {
    namespace {
        constexpr bool k_enable_test_tone{ CARROT_TEST_TONE };
        constexpr bool k_enable_test_sweep{ CARROT_TEST_SWEEP };

        constexpr double k_test_frequency{ 440.0 }; // A4
        constexpr double k_start_freq{ 20.0 };
        constexpr double k_end_freq{ 20000.0 };
        constexpr double k_duration{ 10.0 }; // seconds
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

        if constexpr (k_enable_test_tone)
        {
            const double sample_rate{ static_cast<double>(_clock->sample_rate()) };
            const double phase_inc{ chlm::pi_2 * k_test_frequency / sample_rate };
            uint32_t index{ 0 };

            for (uint32_t frame{ 0 }; frame < frame_count; ++frame)
            {
                const float sample{ .2f * static_cast<float>(std::sin(_phase)) };

                _phase += phase_inc;
                if (_phase >= chlm::pi_2)
                    _phase -= chlm::pi_2;

                for (uint32_t ch = 0; ch < channel_count; ++ch)
                    output[index++] = sample;
            }

            return;
        }

        if constexpr (k_enable_test_sweep)
        {
            const double sample_rate{ static_cast<double>(_clock->sample_rate()) };
            const double dt{ 1.0 / sample_rate };
            uint32_t index{ 0 };

            for (uint32_t frame{ 0 }; frame < frame_count; ++frame)
            {
                const double t{ _sweep_time / k_duration };
                const double freq{ k_start_freq * std::pow(k_end_freq / k_start_freq, t) };
                const double phase_inc{ chlm::pi_2 * freq * dt };
                const float sample{ 0.2f * static_cast<float>(std::sin(_phase)) };

                _phase += phase_inc;
                if (_phase >= chlm::pi_2)
                    _phase -= chlm::pi_2;

                _sweep_time += dt;
                if (_sweep_time > k_duration)
                    _sweep_time = k_duration;

                for (uint32_t ch = 0; ch < channel_count; ++ch)
                    output[index++] = sample;
            }

            return;
        }

        const uint32_t total = frame_count * channel_count;
        for (uint32_t i = 0; i < total; ++i)
            output[i] = 0.0f;
    }
} // namespace carrot::audio
