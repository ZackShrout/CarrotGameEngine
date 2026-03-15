//
// Created by zshrout on 2/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "LinuxAudioBackend.h"

#include <pulse/simple.h>
#include <pulse/error.h>
#include <alsa/asoundlib.h>

namespace carrot::audio {
    namespace {
        const char* backend_name(const backend_kind_t kind) noexcept
        {
            switch (kind)
            {
                case backend_kind_t::pulse: return "PulseAudio/PipeWire";
                case backend_kind_t::alsa: return "ALSA";
                default: return "None";
            }
        }
    } // anonymous namespace
    // PUBLIC
    bool linux_audio_backend_t::init(audio_callback_t* callback, const uint32_t sample_rate, const uint32_t block_size,
                                     const uint32_t channels) noexcept
    {
        LOG_AUDIO_INFO("Initializing Linux audio backend...");

        _callback = callback;
        _sample_rate = sample_rate;
        _block_size = block_size != 0 ? block_size : 512;
        _channels = channels != 0 ? channels : 2;

        // Hard requirement for now: stereo float
        if (_channels != 2)
        {
            LOG_AUDIO_ERROR("Linux backend currently expects stereo (2 channels), got {}", _channels);
            return false;
        }

        // Try PulseAudio / PipeWire first
        if (init_pulse(_sample_rate, _block_size, _channels))
        {
            _backend = backend_kind_t::pulse;
            LOG_AUDIO_INFO("Linux audio backend using {}", backend_name(_backend));
        }
        else
        {
            if (init_alsa(_sample_rate, _block_size, _channels))
            {
                _backend = backend_kind_t::alsa;
                LOG_AUDIO_INFO("Linux audio backend using {}", backend_name(_backend));
            }
            else
            {
                LOG_AUDIO_FATAL("Failed to initialize any Linux audio backend (Pulse/ALSA)");
                _backend = backend_kind_t::none;
                return false;
            }
        }

        // Pre-allocate temp buffer once; reused by audio thread
        const uint32_t frames_per_chunk{ _block_size };
        const uint32_t samples_per_chunk{ frames_per_chunk * _channels };
        _temp_buffer.resize(samples_per_chunk);

        LOG_AUDIO_INFO("Linux audio backend initialized: sample_rate={} Hz, block_size={} frames, channels={}",
                       _sample_rate, _block_size, _channels);

        return true;
    }

    void linux_audio_backend_t::start() noexcept
    {
        if (_backend == backend_kind_t::none || _callback == nullptr)
        {
            LOG_AUDIO_WARN("linux_audio_backend_t::start() called without valid backend");
            return;
        }

        if (_running.exchange(true))
            return; // already running

        _thread = std::thread(&linux_audio_backend_t::audio_thread_proc, this);
    }

    void linux_audio_backend_t::stop() noexcept
    {
        if (!_running.exchange(false))
            return;

        if (_thread.joinable())
            _thread.join();
    }

    void linux_audio_backend_t::shutdown() noexcept
    {
        stop();

        if (_pa_stream)
        {
            int error = 0;
            // Best effort drain
            pa_simple_drain(_pa_stream, &error);
            pa_simple_free(_pa_stream);
            _pa_stream = nullptr;
        }

        if (_alsa_handle)
        {
            snd_pcm_drop(_alsa_handle);
            snd_pcm_close(_alsa_handle);
            _alsa_handle = nullptr;
        }

        _backend = backend_kind_t::none;
        _callback = nullptr;
        _temp_buffer.clear();

        LOG_AUDIO_INFO("Linux audio backend shut down");
    }

    // PRIVATE
    bool linux_audio_backend_t::init_pulse(uint32_t sample_rate, uint32_t block_size, uint32_t channels) noexcept
    {
        pa_sample_spec spec{ };
        spec.format = PA_SAMPLE_FLOAT32LE;
        spec.rate = sample_rate;
        spec.channels = static_cast<uint8_t>(channels);

        int error{ 0 };
        _pa_stream = pa_simple_new(
            /* server      */ nullptr,
            /* app name    */ "CarrotEngine",
            /* dir         */ PA_STREAM_PLAYBACK,
            /* device      */ nullptr, // default sink
            /* stream name */ "Game Audio",
            /* sample spec */ &spec,
            /* channel map */ nullptr,
            /* buffering   */ nullptr,
            /* error       */ &error);

        if (!_pa_stream)
        {
            LOG_AUDIO_WARN("PulseAudio init failed: {}", pa_strerror(error));
            return false;
        }

        // With pa_simple we can't easily query the final hardware rate;
        // assume the server accepted our requested sample rate.
        LOG_AUDIO_INFO("PulseAudio stream created: {} Hz, {} channels, block_size={} frames",
                       sample_rate, channels, block_size);

        return true;
    }

    bool linux_audio_backend_t::init_alsa(uint32_t sample_rate, const uint32_t block_size,
                                          const uint32_t channels) noexcept
    {
        snd_pcm_t* handle{ nullptr };

        int err{ snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0) };
        if (err < 0)
        {
            LOG_AUDIO_WARN("ALSA: snd_pcm_open failed: {}", snd_strerror(err));
            return false;
        }

        snd_pcm_hw_params_t* hw_params{ nullptr };
        snd_pcm_hw_params_alloca(&hw_params);

        err = snd_pcm_hw_params_any(handle, hw_params);
        if (err < 0)
        {
            LOG_AUDIO_ERROR("ALSA: hw_params_any failed: {}", snd_strerror(err));
            snd_pcm_close(handle);
            return false;
        }

        // Interleaved access
        err = snd_pcm_hw_params_set_access(handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
        if (err < 0)
        {
            LOG_AUDIO_ERROR("ALSA: set_access failed: {}", snd_strerror(err));
            snd_pcm_close(handle);
            return false;
        }

        // Prefer float32, fallback to S16 if needed
        snd_pcm_format_t fmt{ SND_PCM_FORMAT_FLOAT_LE };
        err = snd_pcm_hw_params_set_format(handle, hw_params, fmt);
        if (err < 0)
        {
            LOG_AUDIO_WARN("ALSA: FLOAT_LE not supported, trying S16_LE: {}", snd_strerror(err));
            fmt = SND_PCM_FORMAT_S16_LE;
            err = snd_pcm_hw_params_set_format(handle, hw_params, fmt);
            if (err < 0)
            {
                LOG_AUDIO_ERROR("ALSA: set_format S16_LE failed: {}", snd_strerror(err));
                snd_pcm_close(handle);
                return false;
            }
        }

        // Channels
        err = snd_pcm_hw_params_set_channels(handle, hw_params, channels);
        if (err < 0)
        {
            LOG_AUDIO_ERROR("ALSA: set_channels failed: {}", snd_strerror(err));
            snd_pcm_close(handle);
            return false;
        }

        // Sample rate (nearest)
        unsigned int rate{ sample_rate };
        err = snd_pcm_hw_params_set_rate_near(handle, hw_params, &rate, nullptr);
        if (err < 0)
        {
            LOG_AUDIO_ERROR("ALSA: set_rate_near failed: {}", snd_strerror(err));
            snd_pcm_close(handle);
            return false;
        }

        if (rate != sample_rate)
        {
            LOG_AUDIO_WARN("ALSA: requested {} Hz, got {} Hz", sample_rate, rate);
            _sample_rate = rate; // update to actual rate; engine can resample
        }

        // Period size ~ requested block size
        snd_pcm_uframes_t period_frames{ block_size };
        err = snd_pcm_hw_params_set_period_size_near(handle, hw_params, &period_frames, nullptr);
        if (err < 0)
        {
            LOG_AUDIO_ERROR("ALSA: set_period_size_near failed: {}", snd_strerror(err));
            snd_pcm_close(handle);
            return false;
        }

        // Buffer size: 4 periods
        snd_pcm_uframes_t buffer_frames{ period_frames * 4 };
        err = snd_pcm_hw_params_set_buffer_size_near(handle, hw_params, &buffer_frames);
        if (err < 0)
        {
            LOG_AUDIO_ERROR("ALSA: set_buffer_size_near failed: {}", snd_strerror(err));
            snd_pcm_close(handle);
            return false;
        }

        err = snd_pcm_hw_params(handle, hw_params);
        if (err < 0)
        {
            LOG_AUDIO_ERROR("ALSA: hw_params apply failed: {}", snd_strerror(err));
            snd_pcm_close(handle);
            return false;
        }

        err = snd_pcm_prepare(handle);
        if (err < 0)
        {
            LOG_AUDIO_ERROR("ALSA: prepare failed: {}", snd_strerror(err));
            snd_pcm_close(handle);
            return false;
        }

        _alsa_handle = handle;
        _alsa_period_frames = static_cast<uint32_t>(period_frames);
        _block_size = _alsa_period_frames; // engine callback chunk

        LOG_AUDIO_INFO("ALSA stream configured: rate={} Hz, channels={}, period={} frames, buffer={} frames, format={}",
                       _sample_rate, _channels,
                       static_cast<uint32_t>(period_frames),
                       static_cast<uint32_t>(buffer_frames),
                       (fmt == SND_PCM_FORMAT_FLOAT_LE ? "float32" : "s16"));

        return true;
    }

    void linux_audio_backend_t::audio_thread_proc() noexcept
    {
        const uint32_t channels{ _channels };

        const uint32_t frames_per_chunk{
            _backend == backend_kind_t::alsa && _alsa_period_frames != 0 ? _alsa_period_frames : _block_size
        };

        const uint32_t samples_per_chunk{ frames_per_chunk * channels };

        // Sanity: resize if something changed (should not reallocate normally)
        if (_temp_buffer.size() < samples_per_chunk)
            _temp_buffer.resize(samples_per_chunk);

        while (_running)
        {
            float* out{ _temp_buffer.data() };

            // Render from engine
            _callback->render(out, frames_per_chunk, channels);

            switch (_backend)
            {
                case backend_kind_t::pulse:
                {
                    const size_t bytes{ samples_per_chunk * sizeof(float) };
                    int error{ 0 };
                    if (pa_simple_write(_pa_stream, out, bytes, &error) < 0)
                    {
                        LOG_AUDIO_ERROR("PulseAudio: write failed: {}", pa_strerror(error));
                        _running = false; // bail out
                    }

                    break;
                }

                case backend_kind_t::alsa:
                {
                    int frames_to_write{ static_cast<int>(frames_per_chunk) };
                    float* ptr{ out };

                    while (frames_to_write > 0 && _running)
                    {
                        snd_pcm_sframes_t written{ snd_pcm_writei(_alsa_handle, ptr, frames_to_write) };

                        if (written == -EPIPE)
                        {
                            // Underrun; recover
                            snd_pcm_prepare(_alsa_handle);
                            continue;
                        }

                        if (written < 0)
                        {
                            LOG_AUDIO_ERROR("ALSA: write failed: {}", snd_strerror(static_cast<int>(written)));
                            _running = false;
                            break;
                        }

                        frames_to_write -= static_cast<int>(written);
                        ptr += static_cast<int>(written) * channels;
                    }

                    break;
                }

                case backend_kind_t::none:
                default:
                    _running = false;
                    break;
            }
        }

        // Best-effort drain for Pulse
        if (_backend == backend_kind_t::pulse && _pa_stream)
        {
            int error{ 0 };
            pa_simple_drain(_pa_stream, &error);
        }
    }
} // namespace carrot::audio
