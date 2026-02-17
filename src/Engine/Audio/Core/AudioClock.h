//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>

namespace carrot::audio {
    /**
     * @brief Authoritative audio time source for the Carrot audio engine.
     *
     * audio_clock_t represents monotonic, frame-based time as seen by the
     * audio thread. It is advanced exactly once per audio callback and is
     * the foundation upon which all DSP, scheduling, and parameter automation
     * are built.
     *
     * The clock is intentionally simple:
     * - No wall-clock queries
     * - No platform dependencies
     * - No synchronization primitives
     *
     * This guarantees deterministic behavior and real-time safety across
     * all supported platforms.
     *
     * @note
     * Ownership of audio_clock_t is strictly limited to the audio thread.
     * It must never be read or written from the game thread or any
     * non-audio callback context.
     */

    class audio_clock_t
    {
    public:
        /**
         * @brief Initializes the audio clock.
         *
         * This function must be called exactly once before the audio engine
         * begins processing audio blocks.
         *
         * @param sample_rate  The output sample rate in Hz (e.g. 44100, 48000).
         * @param block_size   The number of frames processed per audio callback.
         *
         * @note
         * This function is expected to be called during audio system startup,
         * before the audio thread begins execution.
         */
        void init(const uint32_t sample_rate, const uint32_t block_size) noexcept
        {
            _sample_rate = sample_rate;
            _block_size = block_size;
            _frame_index = 0;
        }

        /**
         * @brief Advances the clock by one audio block.
         *
         * This function must be called exactly once by the audio engine
         * after each successful audio processing callback.
         *
         * Advancing the clock is a constant-time operation and performs
         * no validation or synchronization.
         */
        void advance() noexcept
        {
            _frame_index += _block_size;
        }

        /**
         * @brief Returns the absolute frame index since audio start.
         *
         * The frame index is monotonically increasing and represents the
         * total number of audio frames processed since initialization.
         *
         * @return Absolute frame index since audio start.
         */
        [[nodiscard]] uint64_t frame_index() const noexcept
        {
            return _frame_index;
        }

        /**
         * @brief Returns the audio sample rate in Hz.
         */
        [[nodiscard]] uint32_t sample_rate() const noexcept
        {
            return _sample_rate;
        }

        /**
         * @brief Returns the number of frames processed per audio block.
         */
        [[nodiscard]] uint32_t block_size() const noexcept
        {
            return _block_size;
        }

        /**
         * @brief Returns the current audio time in seconds.
         *
         * This value is derived from the absolute frame index and the
         * configured sample rate. It is intended for debugging and
         * diagnostics only.
         *
         * @warning
         * This function performs floating-point division and must not be
         * used in hot DSP paths.
         */
        [[nodiscard]] double time_seconds() const noexcept
        {
            return static_cast<double>(_frame_index) / static_cast<double>(_sample_rate);
        }

    private:
        uint64_t _frame_index{ 0 };
        uint32_t _sample_rate{ 0 };
        uint32_t _block_size{ 0 };
    };
} // namespace carrot::audio
