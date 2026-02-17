//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "AudioClock.h"
#include "Audio/Backend/AudioBackend.h"
#include "AudioCommandQueue.h"
#include "AudioEventQueue.h"
#include "Audio/Voice/Voice.h"
#include "Audio/Core/AudioCore.h"
#include "Audio/Mixer/AudioMixer.h"

namespace carrot::audio {
    /**
     * @brief Listener state used for spatial audio calculations.
     *
     * Represents the position and orientation of the listener in world space.
     * This data is consumed by spatialization and attenuation code during audio
     * rendering.
     *
     * @note
     * This structure currently exists as a placeholder. In the future, listener
     * state will be driven by the camera or ECS system.
     */
    struct audio_listener_t
    {
        chlm::float3 position{ 0.f, 0.f, 0.f };
        chlm::float3 forward{ 0.f, 1.f, 0.f }; // normalized
    };

    /**
     * @brief Core real-time audio engine.
     *
     * audio_engine_t owns and executes all real-time audio processing state,
     * including active voices, mixing, spatialization, and DSP execution.
     *
     * The engine is driven exclusively by the audio backend via the
     * audio_callback_t interface. All functions that perform real-time audio
     * processing are executed on the audio thread.
     *
     * The engine communicates with the rest of the application through
     * lock-free command and event queues:
     *  - Engine thread → audio thread: audio_command_queue_t
     *  - Audio thread → engine thread: audio_event_queue_t
     *
     * @note
     * After initialization, audio_engine_t is accessed exclusively from the
     * audio thread, with the exception of enqueue_command() and pop_event(),
     * which are thread-safe and intended for cross-thread communication.
     */
    class audio_engine_t final : public audio_callback_t
    {
    public:
        audio_engine_t() = default;
        ~audio_engine_t() = default;

        /**
         * @brief Initializes the audio engine core.
         *
         * This function prepares all real-time audio state, including the voice
         * table, mixer, and timing references. It must be called exactly once
         * before audio rendering begins.
         *
         * @param clock    Pointer to the audio clock owned by the audio module
         * @param channels Number of output channels provided by the backend
         */
        void init(audio_clock_t* clock, uint32_t channels) noexcept;

        /**
         * @brief Shuts down the audio engine core.
         *
         * Stops all audio processing and releases internal state. After this
         * call, the engine may be reinitialized by calling init() again.
         *
         * @note
         * This function is not real-time safe and must not be called from the
         * audio thread while rendering is active.
         */
        void shutdown() noexcept;

        /**
         * @brief Renders a block of audio frames.
         *
         * This function is invoked by the audio backend from the real-time audio
         * thread. It is responsible for consuming queued commands, advancing
         * active voices, mixing audio into the output buffer, and emitting audio
         * events.
         *
         * @param output        Pointer to the interleaved output buffer
         * @param frame_count   Number of frames to render
         * @param channel_count Number of output channels
         */
        void render(float* output, uint32_t frame_count, uint32_t channel_count) noexcept override;

        /**
         * @brief Enqueues a command for execution on the audio thread.
         *
         * This function is thread-safe and may be called from the engine thread.
         * Commands are consumed and executed asynchronously during audio
         * rendering.
         *
         * @param cmd Audio command to enqueue
         * @return True if the command was successfully enqueued, false if the
         *         command queue was full.
         */
        bool enqueue_command(const audio_command_t& cmd) noexcept;

        /**
         * @brief Pops an audio event emitted by the audio thread.
         *
         * This function is thread-safe and intended to be called from the engine
         * thread. Events are emitted by the audio thread to notify the engine of
         * lifecycle changes such as voice completion.
         *
         * @param out Event structure to populate
         * @return True if an event was available, false otherwise.
         */
        bool pop_event(audio_event_t& out) noexcept;

    private:
        /**
         * @brief Consumes and executes all pending audio commands.
         *
         * This function is called from the audio thread during rendering and
         * processes commands enqueued by the engine thread.
         */
        void consume_commands() noexcept;

        /**
         * @brief Finds an active voice by handle.
         *
         * Validates the handle and returns a pointer to the corresponding voice
         * if it is still alive.
         *
         * @param handle Voice handle to resolve
         * @return Pointer to the voice, or nullptr if the handle is invalid or stale.
         */
        voice_t* find_voice(const voice_handle_t& handle) noexcept;

        /**
         * @brief Activates a voice for playback.
         *
         * Initializes runtime state and inserts the voice into the mixer graph.
         *
         * @param voice Voice to activate
         */
        void activate_voice(voice_t& voice) const noexcept;

        audio_clock_t*              _clock{ nullptr };
        audio_mixer_t               _mixer{ };
        uint32_t                    _channels{ 0 };
        uint64_t                    _current_frame{ 0 };

        // NOTE: Temporary until ECS/camera integration
        audio_listener_t            _listener{ };

        // Cross-thread communication
        audio_command_queue_t<256>  _command_queue;
        audio_event_queue_t<256>    _event_queue;

        // Fixed voice table indexed by voice_handle_t::index
        voice_t                     _voices[k_max_voices]{ };
    };
} // namespace carrot::audio
