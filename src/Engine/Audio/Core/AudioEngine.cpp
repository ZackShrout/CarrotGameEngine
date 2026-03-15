//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "AudioEngine.h"

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

    void audio_engine_t::init(audio_clock_t* clock, const uint32_t channels, const uint32_t device_sample_rate) noexcept
    {
        _clock = clock;
        _channels = channels;
        _current_frame = 0;
        _device_sample_rate = device_sample_rate;

        _mixer.init(clock->block_size(), channels);

        _master_ring.init(channels);

        // FX TESTS
        _music_underwater_lp.set_freq(800.f);
        _music_underwater_lp.set_q(0.707f);
        _music_underwater_lp.set_gain(0.f);

        _megaphone_fx_peak.set_freq(1200.f);
        _megaphone_fx_peak.set_q(4.f);
        _megaphone_fx_peak.set_gain(12.f);

        // BASIC OBVIOUS ECHO
        _music_delay.set_delay_ms(400.f);
        _music_delay.set_feedback(0.4f);
        _music_delay.set_wet(0.5f);
        _music_delay.set_dry(1.f);

        // SLAP-BACK
        // _music_delay.set_delay_ms(120.0f);
        // _music_delay.set_feedback(0.15f);
        // _music_delay.set_wet(0.25f);
        // _music_delay.set_dry(1.0f);

        // REVERB-LIKE
        // _music_delay.set_delay_ms(60.0f);
        // _music_delay.set_feedback(0.7f);
        // _music_delay.set_wet(0.7f);
        // _music_delay.set_dry(0.8f);

        // PAIR WITH UNDERWATER
        // _music_delay.set_delay_ms(250.0f);
        // _music_delay.set_feedback(0.5f);
        // _music_delay.set_wet(0.5f);
        // _music_delay.set_dry(1.0f);
    }

    void audio_engine_t::shutdown() noexcept
    {
        _mixer.shutdown();
        _clock = nullptr;
        _channels = 0;
    }

    void audio_engine_t::render(float* output, const uint32_t device_frame_count, const uint32_t channel_count) noexcept
    {
        consume_commands();

        // Fast path: device sample rate & engine sample rate agree!
        if (_device_sample_rate == k_engine_sample_rate)
        {
            mix_engine_frames(device_frame_count);

            const float* master{ _mixer.master_buffer() };
            const uint32_t total_samples{ device_frame_count * channel_count };

            std::memcpy(output, master, total_samples * sizeof(float));
            return;
        }

        // Device sample rate and engine sample rate do not match - run through the resampler
        render_with_master_resampler(output, device_frame_count, channel_count);
    }

    bool audio_engine_t::enqueue_command(const audio_command_t& cmd) noexcept
    {
        return _command_queue.push(cmd);
    }

    bool audio_engine_t::pop_event(audio_event_t& out) noexcept
    {
        return _event_queue.pop(out);
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
                    if (cmd.play_sound.handle.index >= std::size(_voices)) break;

                    voice_t& voice{ _voices[cmd.play_sound.handle.index] };
                    voice.generation++;
                    voice.handle = cmd.play_sound.handle;
                    voice.generation = cmd.play_sound.handle.generation;
                    voice.type = voice_type::sample;
                    voice.sample = cmd.play_sound.sample;
                    voice.pitch = 1.f;
                    voice.src_pos = 0.;
                    const double source_rate{ static_cast<double>(voice.sample->sample_rate) };
                    const double engine_rate{ static_cast<double>(_clock->sample_rate()) };
                    voice.src_step = (source_rate / engine_rate) * static_cast<double>(voice.pitch);

                    // voice.sample_cursor = 0;
                    voice.gain = cmd.play_sound.gain;
                    voice.pan = cmd.play_sound.pan;
                    voice.bus = cmd.play_sound.bus;
                    voice.looping = cmd.play_sound.looping;
                    voice.loop_start = cmd.play_sound.loop_start;
                    voice.loop_end = cmd.play_sound.loop_end;

                    voice.spatial = cmd.play_sound.spatial;
                    voice.position = cmd.play_sound.position;

                    activate_voice(voice);

                    break;
                }

                case audio_command_type::play_stream:
                {
                    voice_t& voice{ _voices[cmd.play_stream.handle.index] };
                    voice.generation++;
                    voice.handle = cmd.play_stream.handle;
                    voice.generation = cmd.play_stream.handle.generation;
                    voice.type = voice_type::stream;
                    voice.stream = cmd.play_stream.stream;

                    voice.bus = cmd.play_stream.bus;
                    voice.spatial = cmd.play_stream.spatial;

                    voice.gain = cmd.play_stream.gain;
                    voice.pan = cmd.play_stream.pan;

                    voice.position = cmd.play_stream.position;
                    voice.max_distance = cmd.play_stream.max_distance;
                    voice.ref_distance = cmd.play_stream.min_distance;

                    voice.looping = cmd.play_stream.looping;

                    if (voice.stream)
                    {
                        voice.stream->looping = cmd.play_stream.looping;
                        voice.stream->eof.store(false, std::memory_order_release);
                        voice.stream->owning_voice = cmd.play_stream.handle;
                        voice.stream->loop_start = cmd.play_stream.loop_start;
                        voice.stream->loop_end = cmd.play_stream.loop_end;
                    }

                    // Stream playback state
                    voice.stream_frames = 0;
                    voice.stream_frame_cursor = 0;
                    voice.stream_channel_cursor = 0;
                    voice.waiting_for_stream = false;

                    activate_voice(voice);

                    break;
                }

                case audio_command_type::stop_all:
                {
                    for (voice_t& v: _voices)
                    {
                        if (v.state == voice_state::active)
                        {
                            envelope_note_off(v.envelope, static_cast<float>(_clock->sample_rate()),
                                              k_default_env.release_seconds);

                            v.state = voice_state::releasing;

                            // Unpause so release can run
                            v.paused = false;
                        }
                    }

                    break;
                }

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

                case audio_command_type::stop_voice:
                {
                    if (voice_t* v{ find_voice(cmd.stop_voice.handle) })
                    {
                        if (v->state == voice_state::active)
                        {
                            envelope_note_off(v->envelope, static_cast<float>(_clock->sample_rate()),
                                              k_default_env.release_seconds);

                            v->state = voice_state::releasing;

                            // Unpause so release can run
                            v->paused = false;
                        }
                    }
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

    void audio_engine_t::render_all_voices_into_buses(const uint32_t engine_frames) noexcept
    {
        uint32_t index{ 0 };

        for (uint32_t frame{ 0 }; frame < engine_frames; ++frame)
        {
            ++_current_frame;
            _clock->advance(1); // advance by 1 engine frame

            for (auto& voice: _voices)
            {
                if (voice.state == voice_state::idle && voice.handle.is_valid())
                {
                    audio_event_t evt{ };
                    evt.type = audio_event_type::voice_finished;
                    evt.handle = voice.handle;
                    _event_queue.push(evt);

                    voice.handle = voice_handle_t::invalid();
                }

                if (voice.state == voice_state::idle)
                    continue;

                const uint32_t src_channels{ voice_source_channels(voice) };
                float distance_gain{ 1.f };
                float spatial_pan{ 0.f };

                // Only spatialize mono sources
                if (src_channels == 1 && voice.spatial != spatial_mode::none)
                {
                    const float dx{ voice.position.x - _listener.position.x };
                    const float dy{ voice.position.y - _listener.position.y };

                    float dist_sq{ dx * dx + dy * dy };

                    if (voice.spatial == spatial_mode::full_3d)
                    {
                        const float dz{ voice.position.z - _listener.position.z };
                        dist_sq += dz * dz;
                    }

                    const float distance{ chlm::sqrt(dist_sq) };
                    distance_gain = distance_attenuation(distance, voice.ref_distance, voice.max_distance);

                    if (voice.spatial == spatial_mode::planar)
                    {
                        const float effective_distance{ chlm::max(distance, voice.ref_distance) };
                        spatial_pan = dx / effective_distance;
                        chlm::clamp(spatial_pan, -1.f, 1.f);
                    }
                }

                const float env{ envelope_tick(voice.envelope) };

                if (env <= 0.0f)
                {
                    voice.state = voice_state::idle;
                    continue;
                }

                float* bus{ _mixer.bus_buffer(voice.bus) };

                if (src_channels == 1)
                {
                    const float raw{ voice_next_sample(voice) };

                    if (raw == 0.0f)
                        continue;

                    float final_pan{ voice.pan };

                    if (voice.spatial == spatial_mode::planar)
                        final_pan += spatial_pan;

                    float pan_l{ 1.f };
                    float pan_r{ 1.f };
                    compute_pan_gains(final_pan, pan_l, pan_r);

                    const float sample{ raw * voice.gain * env * distance_gain };

                    bus[index + 0] += sample * pan_l;
                    bus[index + 1] += sample * pan_r;
                }
                else if (src_channels == 2)
                {
                    float raw_l{ 0.f };
                    float raw_r{ 0.f };
                    voice_next_stereo_frame(voice, raw_l, raw_r);

                    if (raw_l == 0.0f && raw_r == 0.0f)
                        continue;

                    // NOTE: Distance gain is 1.f, as we only compute it for mono spatial voices above
                    const float sample_gain{ voice.gain * env * distance_gain };

                    bus[index + 0] += raw_l * sample_gain;
                    bus[index + 1] += raw_r * sample_gain;
                }
                else
                {
                    // For now, ignore >2 channel content
                    continue;
                }
            }

            index += _channels;
        }
    }

    void audio_engine_t::mix_engine_frames(const uint32_t engine_frames) noexcept
    {
        _mixer.clear(engine_frames);

        render_all_voices_into_buses(engine_frames);

        _mixer.set_bus_reverb_send(audio_bus_id::music, 0.f);

        _mixer.accumulate_reverb_send(engine_frames);

        //—— TEMPORARY FX TESTS ————————————————————————————————————————————
        if (_enable_underwater_music)
        {
            dsp_process_context_t ctx{ };
            ctx.interleaved   = _mixer.bus_buffer(audio_bus_id::music);
            ctx.num_channels  = _channels;
            ctx.num_frames    = engine_frames;
            ctx.sample_rate   = k_engine_sample_rate;

            _music_underwater_lp.process(ctx);
        }

        if (_enable_megaphone_fx)
        {
            dsp_process_context_t ctx{ };
            ctx.interleaved   = _mixer.bus_buffer(audio_bus_id::music);
            ctx.num_channels  = _channels;
            ctx.num_frames    = engine_frames;
            ctx.sample_rate   = k_engine_sample_rate;

            _megaphone_fx_peak.process(ctx);
        }

        if (_enable_delay)
        {
            dsp_process_context_t ctx{ };
            ctx.interleaved   = _mixer.bus_buffer(audio_bus_id::music);
            ctx.num_channels  = _channels;
            ctx.num_frames    = engine_frames;
            ctx.sample_rate   = k_engine_sample_rate;

            _music_delay.process(ctx);
        }
        //—— END TEMPORARY FX TESTS ————————————————————————————————————————

        _mixer.process_bus_fx(engine_frames, k_engine_sample_rate);

        // Mix buses into master (engine rate)
        _mixer.mix_bus_into_master(audio_bus_id::music, engine_frames);
        _mixer.mix_bus_into_master(audio_bus_id::sfx, engine_frames);
        _mixer.mix_bus_into_master(audio_bus_id::ui, engine_frames);
        _mixer.mix_bus_into_master(audio_bus_id::reverb, engine_frames);

        _mixer.process_master_fx(engine_frames, k_engine_sample_rate);
    }

    void audio_engine_t::render_with_master_resampler(float* output, const uint32_t device_frames,
                                                      [[maybe_unused]] const uint32_t device_channels) noexcept
    {
        const double ratio{ static_cast<double>(k_engine_sample_rate) / static_cast<double>(_device_sample_rate) };

        // How many engine frames do we need to generate `device_frames` at this ratio?
        const uint32_t engine_frames_needed{ static_cast<uint32_t>(std::ceil(device_frames * ratio)) };

        // Ensure we have enough engine frames in the master ring.
        while (_master_ring.available_read() < engine_frames_needed)
        {
            constexpr uint32_t k_engine_block{ 256 };

            // Make sure we don't push more than the ring can handle in one go
            // (your ring capacity template param must be >= k_engine_block).
            mix_engine_frames_and_push_to_ring(k_engine_block);
        }

        // Pull exactly the needed engine frames into a temp buffer
        float engine_chunk[engine_frames_needed * 2]; // stereo

        const uint32_t engine_frames_read{ _master_ring.read(engine_chunk, engine_frames_needed) };

        // NOTE: This is paranoia, but if we ever fail to read enough, zero the output and bail.
        if (engine_frames_read < engine_frames_needed)
        {
            const uint32_t total_samples{ device_frames * device_channels };

            for (uint32_t i{ 0 }; i < total_samples; ++i)
                output[i] = 0.0f;

            return;
        }

        // Resample from engine_chunk -> device output
        resample_request_t req{ };
        req.data = engine_chunk;
        req.total_frames = engine_frames_read;
        req.channels = 2;
        req.src_pos = 0.0; // IMPORTANT: per-callback local position
        req.src_step = ratio; // src frames (engine) per dst frame (device)
        req.looping = false;
        req.loop.start = 0;
        req.loop.end = engine_frames_read;

        for (uint32_t i{ 0 }; i < device_frames; ++i)
        {
            float l{ 0.f };
            float r{ 0.f };

            if (!resample_linear_frame(req, l, r))
            {
                // Shouldn't happen if engine_frames_needed math is correct,
                // but we'll fail gracefully.
                output[i * 2 + 0] = 0.f;
                output[i * 2 + 1] = 0.f;

                continue;
            }

            output[i * 2 + 0] = l;
            output[i * 2 + 1] = r;
        }
    }

    void audio_engine_t::mix_engine_frames_and_push_to_ring(const uint32_t engine_frames) noexcept
    {
        mix_engine_frames(engine_frames);

        const float* master = _mixer.master_buffer();
        _master_ring.write(master, engine_frames);
    }
} // namespace carrot::audio
