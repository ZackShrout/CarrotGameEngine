//
// Created by zshrout on 2/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once
#include "Audio/Backend/AudioBackend.h"

#include <atomic>
#include <thread>
#include <vector>

struct pa_simple;
struct _snd_pcm;

namespace carrot::audio {
    enum class backend_kind_t : uint8_t
    {
        none,
        pulse,
        alsa
    };

    /**
     * @brief Linux audio backend (PulseAudio / PipeWire with ALSA fallback).
     *
     * Primary path uses the PulseAudio simple API, which also works when the
     * system is running PipeWire with the Pulse compatibility layer.
     *
     * If PulseAudio initialization fails, the backend falls back to a direct
     * ALSA PCM stream.
     *
     * All audio rendering occurs on a dedicated real-time thread that invokes
     * audio_callback_t::render() in fixed-size blocks.
     */
    class linux_audio_backend_t final : public audio_backend_t
    {
    public:
        linux_audio_backend_t() = default;
        ~linux_audio_backend_t() override = default;

        bool init(audio_callback_t* callback, uint32_t sample_rate, uint32_t block_size,
                  uint32_t channels) noexcept override;

        void start() noexcept override;
        void stop() noexcept override;
        void shutdown() noexcept override;

        [[nodiscard]] uint32_t sample_rate() const noexcept override { return _sample_rate; }
        [[nodiscard]] uint32_t block_size() const noexcept override { return _block_size; }
        [[nodiscard]] uint32_t channel_count() const noexcept override { return _channels; }

    private:
        // Backend-specific init
        bool init_pulse(uint32_t sample_rate, uint32_t block_size, uint32_t channels) noexcept;
        bool init_alsa(uint32_t sample_rate, uint32_t block_size, uint32_t channels) noexcept;

        void audio_thread_proc() noexcept;

        // Backend selection / state
        backend_kind_t _backend{ backend_kind_t::none };

        audio_callback_t* _callback{ nullptr }; // non-owning

        // Common config
        uint32_t _sample_rate{ 0 };
        uint32_t _block_size{ 0 };   // frames per callback into engine
        uint32_t _channels{ 0 };

        // PulseAudio state
        pa_simple* _pa_stream{ nullptr };

        // ALSA state
        _snd_pcm* _alsa_handle{ nullptr };
        uint32_t  _alsa_period_frames{ 0 };

        // Threading
        std::thread _thread;
        std::atomic<bool> _running{ false };

        // Pre-allocated temp buffer for audio thread (float interleaved)
        std::vector<float> _temp_buffer;
    };
} // namespace carrot::audio
