//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>
#include <memory>

namespace carrot::audio {
    /**
     * @brief Callback interface invoked by the audio backend.
     *
     * Implemented by the audio engine core and called from the audio backend's
     * real-time audio thread whenever a new block of audio must be rendered.
     *
     * @note
     * All functions are called from the audio thread and must be:
     *  - real-time safe
     *  - non-blocking
     *  - allocation-free
     *  - noexcept
     */
    struct audio_callback_t
    {
    public:
        /**
         * @brief Renders a block of audio frames.
         *
         * @param output        Pointer to interleaved output buffer
         * @param frame_count   Number of frames to render
         * @param channel_count Number of output channels
         *
         * @note
         * The output buffer is guaranteed to be writable and large enough
         * for (frame_count * channel_count) samples.
         */
        virtual void render(float* output, uint32_t frame_count, uint32_t channel_count) noexcept = 0;

    protected:
        ~audio_callback_t() = default;
    };

    /**
     * @brief Abstract interface for platform-specific audio backends.
     *
     * audio_backend_t is responsible for interfacing with the host operating
     * system's audio API (WASAPI, ALSA, CoreAudio, etc.) and invoking the
     * audio callback at regular intervals.
     *
     * Implementations must be thin, deterministic, and free of engine logic.
     *
     * @note
     * Backends are owned and managed by audio_module_t and are not thread-safe
     * unless explicitly documented otherwise.
     */
    struct audio_backend_t
    {
    public:
        virtual ~audio_backend_t() = default;

        /**
         * @brief Initializes the audio backend.
         *
         * This function performs device selection, stream configuration,
         * and any backend-specific setup required before audio playback
         * can begin.
         *
         * @param callback      Audio callback to invoke from the audio thread
         * @param sample_rate   Requested output sample rate (Hz)
         * @param block_size    Requested number of frames per audio callback
         * @param channels      Requested number of output channels
         *
         * @return true if initialization succeeded, false otherwise
         *
         * @note
         * The backend may adjust the requested parameters to match the
         * capabilities of the underlying audio device.
         */
        virtual bool init(
            audio_callback_t* callback,
            uint32_t sample_rate,
            uint32_t block_size,
            uint32_t channels) noexcept = 0;

        /**
         * @brief Starts audio playback.
         *
         * After this call, the backend will begin invoking the audio callback
         * from its real-time audio thread.
         */
        virtual void start() noexcept = 0;

        /**
         * @brief Stops audio playback.
         *
         * After this call returns, the backend must guarantee that no further
         * audio callbacks will be invoked.
         */
        virtual void stop() noexcept = 0;

        /**
         * @brief Shuts down the audio backend and releases all resources.
         *
         * Must leave the backend in a state where init() may be called again.
         */
        virtual void shutdown() noexcept = 0;

        /**
         * @brief Returns the actual output sample rate in Hz.
         *
         * This value may differ from the requested sample rate passed to init().
         */
        [[nodiscard]] virtual uint32_t sample_rate() const noexcept = 0;

        /**
         * @brief Returns the actual number of frames per audio callback.
         *
         * This value may differ from the requested block size passed to init().
         */
        [[nodiscard]] virtual uint32_t block_size() const noexcept = 0;

        /**
         * @brief Returns the actual number of output channels.
         */
        [[nodiscard]] virtual uint32_t channel_count() const noexcept = 0;
    };

    [[nodiscard]] std::unique_ptr<audio_backend_t> create_audio_backend();
} // namespace carrot::audio
