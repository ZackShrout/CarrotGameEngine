//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/Module.h"
#include "AudioClock.h"
#include "EngineConfig.h"
#include "Audio/Backend/AudioBackend.h"
#include "Audio/Core/AudioEngine.h"
#include "Audio/Core/AudioCommand.h"

#include <memory>
#include <thread>

namespace carrot::audio {
    struct audio_backend_t;
    class audio_engine_t;
    // class audio_command_queue_t;

    /**
     * @brief Top-level audio subsystem module.
     *
     * audio_module_t is responsible for owning and orchestrating the entire
     * audio system. It manages backend initialization, audio thread lifetime,
     * command routing, and clean shutdown.
     *
     * This module runs on the engine thread and must never perform real-time
     * audio processing directly.
     *
     * @note
     * All DSP, mixing, and graph execution occur exclusively on the audio thread.
     * audio_module_t exists solely to manage lifecycle and cross-thread communication.
     */
    class audio_module_t final : public core::module_t
    {
    public:
        CARROT_MODULE_NAME("Audio")

        explicit audio_module_t(const engine_audio_config_t& config);
        ~audio_module_t() override = default;

        DISABLE_COPY(audio_module_t)

        /**
         * @brief Initializes the audio subsystem.
         *
         * This function:
         *  - selects and initializes the audio backend
         *  - creates command queues
         *  - initializes the audio clock
         *  - starts the audio thread
         *
         * @note Called exactly once during engine startup (or hot-reload).
         */
        void init() override;

        /**
         * @brief Shuts down the audio subsystem.
         *
         * This function:
         *  - signals the audio thread to stop
         *  - waits for audio thread termination
         *  - shuts down the backend
         *  - releases all owned resources
         *
         * @note Must leave the module in a state where init() may be called again.
         */
        void shutdown() override;

        [[nodiscard]] audio_engine_t& engine() const noexcept { return *_engine; }

    private:
        /**
         * @brief Entry point for the audio thread.
         *
         * Runs the real-time audio loop, processing audio blocks and advancing
         * the audio clock. This function must be fully real-time safe.
         */
        void audio_thread_main() noexcept;

        engine_audio_config_t                   _config;

        std::unique_ptr<audio_backend_t>        _backend;
        std::unique_ptr<audio_engine_t>         _engine;
        // std::unique_ptr<audio_command_queue_t>  _command_queue;

        audio_clock_t                           _clock;

        std::thread                             _audio_thread;
        bool                                    _should_run{ false };
    };
} // namespace carrot::audio
