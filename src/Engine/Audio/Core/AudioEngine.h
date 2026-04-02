//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Audio/Backend/AudioBackend.h"
#include "Audio/Core/AudioCore.h"
#include "Audio/DSP/BiquadFilter.h"
#include "Audio/DSP/Delay.h"
#include "Audio/DSP/SchroederReverb.h"
#include "Audio/Mixer/AudioMixer.h"
#include "Audio/Voice/Voice.h"
#include "AudioClock.h"
#include "AudioCommandQueue.h"
#include "AudioEventQueue.h"

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
         * @param clock Pointer to the audio clock owned by the audio module
         * @param channels Number of output channels provided by the backend
         * @param device_sample_rate Sample rate of the audio device used by backend
         */
        void init(audio_clock_t* clock, uint32_t channels, uint32_t device_sample_rate) noexcept;

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
         * @param device_frame_count   Number of frames to render
         * @param channel_count Number of output channels
         */
        void render(float* output, uint32_t device_frame_count, uint32_t channel_count) noexcept override;

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

        /**
         * @brief Renders all active voices into audio buses for the given number of engine frames.
         *
         * Processes audio voices, applying spatialization, envelopes, and mixing into the appropriate
         * output buses for each frame in the specified duration. This function handles different
         * voice states, channels, and spatial configurations to ensure proper audio rendering.
         *
         * @param engine_frames The number of engine frames to process during this rendering step.
         */
        void render_all_voices_into_buses(uint32_t engine_frames) noexcept;

        /**
         * @brief Mixes audio frames for the engine into the master output.
         *
         * This method processes and mixes all active audio sources, applies effects,
         * and outputs the finalized audio frames into the master buffer. It also handles
         * per-bus FX processing.
         *
         * @param engine_frames The number of engine frames to process during this rendering step.
         */
        void mix_engine_frames(uint32_t engine_frames) noexcept;

        /**
         * @brief Renders audio output using the master resampler.
         *
         * This method resamples audio data from the engine's sample rate to the device's
         * sample rate and mixes audio into the provided output buffer. It ensures enough
         * audio frames are generated and resampled before outputting the processed data.
         * The resampling is performed per device frame based on the engine-to-device sample
         * rate ratio.
         *
         * @param output Pointer to the buffer where the resampled audio data will be written.
         * @param device_frames Number of audio frames to be written to the output buffer.
         * @param device_channels Number of audio channels for the device (unused in this method).
         */
        void render_with_master_resampler(float* output, uint32_t device_frames, uint32_t device_channels) noexcept;

        /**
         * @brief Mixes audio engine frames and pushes the resulting data to the master ring buffer.
         *
         * Combines audio frames processed by the engine and writes the mixed output to the
         * master ring buffer for further playback or processing.
         *
         * @param engine_frames The number of audio frames to mix and push to the ring buffer.
         */
        void mix_engine_frames_and_push_to_ring(uint32_t engine_frames) noexcept;

        audio_clock_t*              _clock{ nullptr };
        audio_mixer_t               _mixer{ };
        uint32_t                    _channels{ 0 };
        uint64_t                    _current_frame{ 0 };
        uint32_t                    _device_sample_rate{ 0 };

        // NOTE: Temporary until ECS/camera integration
        audio_listener_t            _listener{ };

        // Cross-thread communication
        audio_command_queue_t<256>  _command_queue;
        audio_event_queue_t<256>    _event_queue;

        // Fixed voice table indexed by voice_handle_t::index
        voice_t                     _voices[k_max_voices]{ };

        // Engine-rate master FIFO (stereo, 48k)
        static constexpr uint32_t k_master_buffer_frames = 2048;
        audio_ring_buffer_t<k_master_buffer_frames, 2> _master_ring;

        // Resampler state from engine -> device
        double _master_src_pos{ 0.0 };

        //—— TEMPORARY FX TEST STATE —————————————————————————————————————————————————————————————
        bool _enable_underwater_music{ false };
        dsp_biquad_filter_t _music_underwater_lp{ biquad_type::lowpass, k_engine_sample_rate };

        bool _enable_megaphone_fx{ false };
        dsp_biquad_filter_t _megaphone_fx_peak{ biquad_type::peak, k_engine_sample_rate };

        bool _enable_delay{ false };
        dsp_delay_line_t _music_delay{ k_engine_sample_rate, 1000u, 2u };
    };
} // namespace carrot::audio
