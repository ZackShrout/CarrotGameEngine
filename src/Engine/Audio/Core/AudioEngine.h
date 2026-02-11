//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Audio/AudioClock.h"
#include "Audio/Backend/AudioBackend.h"

#include <cstdint>

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

    private:
        audio_clock_t*  _clock{ nullptr };
        uint32_t        _channels{ 0 };
    };
} // namespace carrot::audio
