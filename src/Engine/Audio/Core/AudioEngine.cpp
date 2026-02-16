//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "AudioEngine.h"

#include <chlm/CarrotHLM.h>

#include "Audio/DSP/Pan.h"

namespace carrot::audio {
    namespace {
        constexpr envelope_params_t k_default_env{
            .attack_seconds = 0.01f,
            .decay_seconds = 0.1f,
            .sustain_level = 0.8f,
            .release_seconds = 0.2f
        };

        float distance_attenuation(const float distance, const float ref_distance, const float max_distance) noexcept
        {
            if (distance <= ref_distance) return 1.f;
            if (distance >= max_distance) return 0.f;

            return ref_distance / distance;
        }
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
        consume_commands();
        _clock->advance();
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

                const float raw{ voice_next_sample(voice, _clock->sample_rate()) };

                if (raw == 0.0f)
                    continue;

                float distance_gain{ 1.f };
                float spatial_pan{ 0.f };

                if (voice.spatial != spatial_mode::none)
                {
                    const float dx{ voice.position.x - _listener.position.x };
                    const float dy{ voice.position.y - _listener.position.y };

                    float dist_sq{ dx * dx + dy * dy };

                    if (voice.spatial == spatial_mode::full_3d)
                    {
                        const float dz{ voice.position.z - _listener.position.z };
                        dist_sq += dz * dz;
                    }

                    const float distance{ std::sqrt(dist_sq) };
                    distance_gain = distance_attenuation(distance, voice.ref_distance, voice.max_distance);

                    if (voice.spatial == spatial_mode::planar)
                    {
                        const float effective_distance{ std::max(distance, voice.ref_distance) };
                        spatial_pan = dx / effective_distance;
                        chlm::clamp(spatial_pan, -1.f, 1.f);
                    }
                }

                float final_pan{ voice.pan };

                if (voice.spatial == spatial_mode::planar)
                    final_pan += spatial_pan;

                float pan_l{ 1.f };
                float pan_r{ 1.f };
                compute_pan_gains(final_pan, pan_l, pan_r);

                const float sample{ raw * voice.gain * env * distance_gain };
                float* bus{ _mixer.bus_buffer(voice.bus) };

                // stereo output assumed for now
                bus[index + 0] += sample * pan_l;
                bus[index + 1] += sample * pan_r;

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
                case audio_command_type::play_sound:
                {
                    if (voice_t* v{ acquire_voice() })
                    {
                        v->generation++;
                        v->handle = cmd.play_sound.handle;
                        v->handle.generation = v->generation;
                        v->type = voice_type::sample;
                        v->sample = cmd.play_sound.sample;
                        v->gain = cmd.play_sound.gain;
                        v->pan = cmd.play_sound.pan;
                        v->bus = cmd.play_sound.bus;
                        v->looping = cmd.play_sound.looping;
                        v->loop_start = cmd.play_sound.loop_start;
                        v->loop_end = cmd.play_sound.loop_end;

                        v->spatial = cmd.play_sound.spatial;
                        v->position = cmd.play_sound.position;

                        activate_voice(*v);
                    }
                    break;
                }

                case audio_command_type::stop_all:
                    _sine_active = false;
                    break;

                case audio_command_type::pause_voice:
                {
                    if (voice_t* v{ find_voice(cmd.pause_voice.handle) })
                        v->paused = true;

                    break;
                }

                case audio_command_type::resume_voice:
                {
                    if (voice_t* v{ find_voice(cmd.resume_voice.handle) })
                        v->paused = false;

                    break;
                }

                case audio_command_type::set_bus_gain:
                    _mixer.set_bus_gain(cmd.set_bus_gain.bus, cmd.set_bus_gain.gain);
                    break;

                case audio_command_type::set_bus_mute:
                    _mixer.set_bus_mute(cmd.set_bus_mute.bus, cmd.set_bus_mute.enabled);
                    break;

                case audio_command_type::set_bus_solo:
                    _mixer.set_bus_solo(cmd.set_bus_solo.bus, cmd.set_bus_solo.enabled);
                    break;

                case audio_command_type::set_bus_pan:
                    _mixer.set_bus_pan(cmd.set_bus_pan.bus, cmd.set_bus_pan.pan);
                    break;

                case audio_command_type::set_voice_pan:
                {
                    if (voice_t* v{ find_voice(cmd.set_voice_pan.handle) })
                        v->pan = cmd.set_voice_pan.pan;

                    break;
                }

                case audio_command_type::set_voice_gain:
                {
                    if (voice_t* v{ find_voice(cmd.set_voice_gain.handle) })
                        v->gain = cmd.set_voice_gain.gain;

                    break;
                }

                case audio_command_type::set_voice_spatial:
                {
                    if (voice_t* v{ find_voice(cmd.set_voice_spatial.handle) })
                        v->spatial = cmd.set_voice_spatial.mode;

                    break;
                }

                case audio_command_type::set_voice_position:
                {
                    if (voice_t* v{ find_voice(cmd.set_voice_position.handle) })
                    {
                        v->position.x = cmd.set_voice_position.x;
                        v->position.y = cmd.set_voice_position.y;
                        v->position.z = cmd.set_voice_position.z;
                    }

                    break;
                }
            }
        }
    }

    // PRIVATE

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

    voice_t* audio_engine_t::acquire_voice() noexcept
    {
        // 1. Prefer idle
        for (auto& v : _voices)
        {
            if (v.state == voice_state::idle)
                return &v;
        }

        // 2. Steal
        voice_t* v{ choose_voice_to_steal() };
        if (!v)
            return nullptr;

        if (v->state != voice_state::releasing)
        {
            envelope_note_off(v->envelope, static_cast<float>(_clock->sample_rate()), k_default_env.release_seconds);
            v->state = voice_state::releasing;
        }

        return v;
    }

    voice_t* audio_engine_t::find_voice(const voice_handle_t& handle) noexcept
    {
        if (handle.index >= std::size(_voices)) return nullptr;
        if (_voices[handle.index].generation != handle.generation) return nullptr;

        return &_voices[handle.index];
    }

    void audio_engine_t::activate_voice(voice_t& voice) const noexcept
    {
        voice.start_frame = _current_frame;
        envelope_note_on(voice.envelope, k_default_env, static_cast<float>(_clock->sample_rate()));
        voice.state = voice_state::active;
    }
} // namespace carrot::audio
