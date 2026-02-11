//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Audio/Backend/AudioBackend.h"

#include <atomic>
#include <thread>

namespace carrot::audio {
    /**
     * @brief Fake audio backend for testing and bring-up.
     *
     * Simulates an audio device by invoking the audio callback from
     * a normal worker thread at approximately real-time intervals.
     *
     * @note
     * This backend is NOT real-time safe and must never be used in production.
     */
    class null_audio_backend_t final : public audio_backend_t
    {
    public:
        null_audio_backend_t() = default;
        ~null_audio_backend_t() override = default;

        bool init(
            audio_callback_t* callback,
            uint32_t sample_rate,
            uint32_t block_size,
            uint32_t channels) noexcept override;

        void start() noexcept override;
        void stop() noexcept override;
        void shutdown() noexcept override;

        [[nodiscard]] uint32_t sample_rate() const noexcept override;
        [[nodiscard]] uint32_t block_size() const noexcept override;
        [[nodiscard]] uint32_t channel_count() const noexcept override;

    private:
        void thread_main();

        audio_callback_t* _callback{ nullptr };

        uint32_t _sample_rate{ 0 };
        uint32_t _block_size{ 0 };
        uint32_t _channels{ 0 };

        std::atomic<bool> _running{ false };
        std::thread _thread;
    };
} // namespace carrot::audio
