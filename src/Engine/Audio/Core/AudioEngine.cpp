//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "AudioEngine.h"

#include <chlm/CarrotHLM.h>

namespace carrot::audio {
    namespace {
        constexpr envelope_params_t k_default_env{
            .attack_seconds = 0.01f,
            .decay_seconds = 0.1f,
            .sustain_level = 0.8f,
            .release_seconds = 0.2f
        };
    } // anonymous namespace
    // PUBLIC

    void audio_engine_t::init(audio_clock_t* clock, const uint32_t channels) noexcept
    {
        _clock = clock;
        _channels = channels;
        _sine_phase = 0.0;
        _current_frame = 0;

        _mixer.init(clock->block_size(), channels);
    }

    void audio_engine_t::shutdown() noexcept
    {
        _mixer.shutdown();
        _clock = nullptr;
        _channels = 0;
    }

    void audio_engine_t::render(float* output, const uint32_t frame_count, const uint32_t channel_count) noexcept
    {
        _clock->advance();
        consume_commands();
        _mixer.clear(frame_count);

        uint32_t index{ 0 };

        for (uint32_t frame{ 0 }; frame < frame_count; ++frame)
        {
            ++_current_frame;

            for (auto& voice : _voices)
            {
                if (voice.state == voice_state::idle)
                    continue;

                const float env{ envelope_tick(voice.envelope) };

                if (env <= 0.0f)
                {
                    voice.state = voice_state::idle;
                    continue;
                }

                const float sample{ voice.gain * static_cast<float>(std::sin(voice.phase)) * env };

                voice.phase += voice.phase_inc;
                if (voice.phase >= chlm::pi_2)
                    voice.phase -= chlm::pi_2;

                float* bus{ _mixer.bus_buffer(voice.bus) };

                for (uint32_t ch = 0; ch < _channels; ++ch)
                    bus[index + ch] += sample;
            }

            index += _channels;
        }

        _mixer.mix_bus_into_master(audio_bus_id::music, frame_count);
        _mixer.mix_bus_into_master(audio_bus_id::sfx, frame_count);
        _mixer.mix_bus_into_master(audio_bus_id::ui, frame_count);

        const float* master{ _mixer.master_buffer() };
        const uint32_t total{ frame_count * channel_count };

        for (uint32_t i = 0; i < total; ++i)
            output[i] = master[i];
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
                {
                    voice_t* chosen = nullptr;

                    // 1. Prefer idle
                    for (auto& v: _voices)
                    {
                        if (v.state == voice_state::idle)
                        {
                            chosen = &v;
                            break;
                        }
                    }

                    // 2. Steal if needed
                    if (!chosen)
                    {
                        chosen = choose_voice_to_steal();

                        if (chosen && chosen->state != voice_state::releasing)
                        {
                            envelope_note_off(chosen->envelope, static_cast<float>(_clock->sample_rate()),
                                              k_default_env.release_seconds);
                            chosen->state = voice_state::releasing;
                        }
                    }

                    // 3. If we got a slot, (re)initialize
                    if (chosen && chosen->state == voice_state::idle)
                    {
                        chosen->frequency = cmd.play_sine.frequency;
                        chosen->gain = cmd.play_sine.gain;
                        chosen->phase = 0.0;
                        chosen->phase_inc = chlm::pi_2 * chosen->frequency / static_cast<double>(_clock->sample_rate());
                        chosen->start_frame = _current_frame;

                        envelope_note_on(chosen->envelope, k_default_env, static_cast<float>(_clock->sample_rate()));

                        chosen->state = voice_state::active;
                    }

                    break;
                }

                case audio_command_type::stop_all:
                    _sine_active = false;
                    break;

                case audio_command_type::set_bus_gain:
                    _mixer.set_bus_gain(cmd.set_bus_gain.bus, cmd.set_bus_gain.gain);
                    break;

                case audio_command_type::set_bus_mute:
                    _mixer.set_bus_mute(cmd.set_bus_mute.bus, cmd.set_bus_mute.enabled);
                    break;

                case audio_command_type::set_bus_solo:
                    _mixer.set_bus_solo(cmd.set_bus_solo.bus, cmd.set_bus_solo.enabled);
                    break;

                case audio_command_type::set_voice_gain:
                {
                    const uint32_t id = cmd.set_voice_gain.voice_index;
                    if (id < std::size(_voices))
                        _voices[id].gain = cmd.set_voice_gain.gain;

                    break;
                }
            }
        }
    }

    voice_t* audio_engine_t::choose_voice_to_steal() noexcept
    {
        voice_t* chosen{ nullptr };
        uint64_t oldest{ UINT64_MAX };

        for (auto& v: _voices)
        {
            if (v.state == voice_state::active || v.state == voice_state::releasing)
            {
                if (v.start_frame < oldest)
                {
                    oldest = v.start_frame;
                    chosen = &v;
                }
            }
        }

        return chosen;
    }
} // namespace carrot::audio
