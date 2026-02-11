//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "AudioEngine.h"

#include <chlm/CarrotHLM.h>

namespace carrot::audio {
    // PUBLIC

    void audio_engine_t::init(audio_clock_t* clock, const uint32_t channels) noexcept
    {
        _clock = clock;
        _channels = channels;
        _sine_phase = 0.0;
    }

    void audio_engine_t::shutdown() noexcept
    {
        _clock = nullptr;
        _channels = 0;
    }

    void audio_engine_t::render(float* output, const uint32_t frame_count, const uint32_t channel_count) noexcept
    {
        _clock->advance();
        consume_commands();

        if (!_sine_active)
        {
            const uint32_t total{ frame_count * channel_count };
            for (uint32_t i = 0; i < total; ++i)
                output[i] = 0.0f;
            return;
        }

        const double sample_rate{ static_cast<double>(_clock->sample_rate()) };
        const double phase_inc{ chlm::pi_2 * _sine_freq / sample_rate };
        uint32_t index{ 0 };

        for (uint32_t frame = 0; frame < frame_count; ++frame)
        {
            const float sample{ _sine_gain * static_cast<float>(std::sin(_sine_phase)) };

            _sine_phase += phase_inc;
            if (_sine_phase >= chlm::pi_2)
                _sine_phase -= chlm::pi_2;

            for (uint32_t ch = 0; ch < channel_count; ++ch)
                output[index++] = sample;
        }
    }

    bool audio_engine_t::enqueue_command(const audio_command_t& cmd) noexcept
    {
        return _command_queue.push(cmd);
    }

    // PRIVATE

    void audio_engine_t::consume_commands() noexcept
    {
        audio_command_t cmd{ };

        while (_command_queue.pop(cmd))
        {
            switch (cmd.type)
            {
                case audio_command_type::play_sine:
                    _sine_active = true;
                    _sine_freq = cmd.play_sine.frequency;
                    _sine_gain = cmd.play_sine.gain;
                    _sine_phase = 0.0;
                    break;

                case audio_command_type::stop_all:
                    _sine_active = false;
                    break;
            }
        }
    }
} // namespace carrot::audio
