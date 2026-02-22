//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/Module.h"
#include "Core/AudioClock.h"
#include "EngineConfig.h"
#include "Audio/Backend/AudioBackend.h"
#include "Audio/Core/AudioEngine.h"

#include <memory>
#include <thread>

#include "Streaming/WavStreamDecoder.h"

namespace carrot::assets {
    struct audio_asset_t;
}

namespace carrot::audio {
    struct audio_backend_t;
    class audio_engine_t;

    /**
     * @brief Top-level audio subsystem module.
     *
     * audio_module_t owns and orchestrates the entire audio system.
     * It is responsible for:
     *  - audio backend creation and shutdown
     *  - audio engine lifetime management
     *  - audio clock ownership
     *  - voice handle allocation and reuse
     *  - cross-thread communication between engine and audio threads
     *
     * This module runs exclusively on the engine thread and must never
     * perform real-time audio processing directly.
     *
     * @note
     * All DSP, mixing, voice processing, and rendering occur exclusively
     * on the audio thread via audio_engine_t::render().
     * audio_module_t exists purely for lifecycle management and
     * thread-safe command/event routing.
     */
    class audio_module_t final : public core::module_t
    {
    public:
        CARROT_MODULE_NAME("Audio")

        /**
         * @brief Constructs the audio module with engine audio configuration.
         *
         * @param config Immutable audio configuration supplied by the engine
         */
        explicit audio_module_t(const engine_audio_config_t& config);

        ~audio_module_t() override = default;

        DISABLE_COPY(audio_module_t)

        /**
         * @brief Initializes the audio subsystem.
         *
         * This function:
         *  - selects and initializes the audio backend
         *  - creates the audio engine
         *  - initializes the audio clock
         *  - starts the real-time audio thread
         *
         * @note
         * Called exactly once during engine startup (or hot-reload).
         * Must not be called from the audio thread.
         */
        void init() override;

        /**
         * @brief Per-frame update for non-real-time audio work.
         *
         * This function is executed on the engine thread and is intended
         * for tasks such as:
         *  - polling audio events from the audio engine
         *  - releasing finished voice handles
         *  - synchronizing non-RT state
         *
         * @param delta_time Time elapsed since last frame (seconds)
         */
        void update(float delta_time) noexcept override;

        /**
         * @brief Shuts down the audio subsystem.
         *
         * This function:
         *  - signals the audio backend to stop
         *  - waits for the audio thread to terminate
         *  - shuts down the audio engine
         *  - releases all owned resources
         *
         * @note
         * Must leave the module in a state where init() may be called again.
         * Must not be called from the audio thread.
         */
        void shutdown() override;

        /**
         * @brief Allocates a new voice handle.
         *
         * Voice handles are allocated on the engine thread and passed to
         * the audio thread via commands. The audio engine determines when
         * a voice has fully finished playback.
         *
         * @return Newly allocated voice handle
         */
        voice_handle_t allocate_voice_handle() noexcept;

        /**
         * @brief Releases a voice handle for reuse.
         *
         * This function invalidates the handle by incrementing its generation
         * and returns the slot to the free list.
         *
         * @param handle Voice handle to release
         */
        void release_voice_handle(voice_handle_t handle) noexcept;

        audio_stream_t* create_stream_from_asset(const assets::audio_asset_t& asset) noexcept;

        /**
         * @brief Accesses the audio engine instance.
         *
         * @return Reference to the audio engine
         *
         * @warning
         * The returned audio_engine_t must never be accessed directly
         * from outside the audio thread except for enqueueing commands
         * or polling events via its public API.
         */
        [[nodiscard]] audio_engine_t& engine() const noexcept { return *_engine; }

    private:
        engine_audio_config_t                   _config;

        std::unique_ptr<audio_backend_t>        _backend;
        std::unique_ptr<audio_engine_t>         _engine;

        /** Audio clock shared with the audio engine. */
        audio_clock_t                           _clock;

        /**
         * @brief Internal voice handle slot.
         *
         * Used to validate handles via generation counters and to
         * recycle indices safely.
         */
        struct voice_slot_t
        {
            uint32_t generation{ 1 };
            bool active{ false };
        };

        /** All allocated voice slots. */
        std::vector<voice_slot_t>               _voice_slots;

        /** Indices of free voice slots available for reuse. */
        std::vector<uint32_t>                   _free_slots;

        audio_stream_t                          _stream;
        wav_stream_decoder_t                    _decoder;
    };
} // namespace carrot::audio
