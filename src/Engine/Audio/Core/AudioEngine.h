//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Audio/AudioClock.h"
#include "Audio/Backend/AudioBackend.h"
#include "AudioCommandQueue.h"
#include "Audio/Voice/Voice.h"
#include "Audio/Core/AudioCore.h"
#include "Audio/Mixer/AudioMixer.h"

namespace carrot::audio {
    /**
     * @brief Core audio engine executed on the audio thread.
     *
     * audio_engine_t owns all real-time audio processing state, including
     * the audio graph, mixer, and DSP execution. It is driven exclusively
     * by the audio backend via the audio_callback_t interface.
     *
     * This initial implementation renders silence and exists to validate
     * threading, timing, and backend integration.
     *
     * @note
     * This object is accessed exclusively from the audio thread after startup.
     */
    class audio_engine_t final : public audio_callback_t
    {
    public:
        audio_engine_t() = default;
        ~audio_engine_t() = default;

        /**
         * @brief Initializes the audio engine core.
         *
         * @param clock Pointer to the audio clock owned by the audio module
         * @param channels Number of output channels
         */
        void init(audio_clock_t* clock, uint32_t channels) noexcept;

        /**
         * @brief Shuts down the audio engine core.
         *
         * Must leave the engine in a state where init() may be called again.
         */
        void shutdown() noexcept;

        /**
         * @brief Renders a block of audio frames.
         *
         * This function is called by the audio backend from the real-time
         * audio thread.
         */
        void render(float* output, uint32_t frame_count, uint32_t channel_count) noexcept override;

        bool enqueue_command(const audio_command_t& cmd) noexcept;

    private:
        void consume_commands() noexcept;
        voice_t* choose_voice_to_steal() noexcept;
        voice_t* acquire_voice() noexcept;
        void activate_voice(voice_t& voice) const noexcept;

        audio_clock_t* _clock{ nullptr };
        audio_mixer_t _mixer{ };
        uint32_t _channels{ 0 };
        uint64_t _current_frame{ 0 };

        audio_command_queue_t<256> _command_queue;
        voice_t _voices[k_max_voices]{ };

        // ── Test sine state (temporary) ─────────────────────────────
        bool _sine_active{ false };
        double _sine_phase{ 0.0 };
        double _sweep_time{ 0.0 };
        double _sine_freq{ 440.0 };
        float _sine_gain{ 0.2f };
    };
} // namespace carrot::audio
