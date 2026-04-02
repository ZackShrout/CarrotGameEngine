//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Audio/Core/AudioCore.h"
#include "Audio/DSP/Envelope.h"
#include "Audio/DSP/Resampler.h"
#include "Audio/Mixer/AudioBus.h"
#include "Audio/Sample/AudioSample.h"
#include "Audio/Streaming/AudioStream.h"
#include "VoiceHandle.h"

#include <chlm/Core.h>

namespace carrot::audio {
    /**
     * @brief Runtime state of a voice instance.
     *
     * Voices transition through these states over their lifetime.
     */
    enum class voice_state : uint8_t
    {
        /** Voice slot is unused and available for allocation. */
        idle,

        /** Voice is actively producing audio. */
        active,

        /**
         * Voice has been stopped and is playing its release envelope.
         *
         * Once the envelope reaches silence, the voice transitions
         * back to @ref idle.
         */
        releasing,
    };

    /**
     * @brief Type of audio source driving a voice.
     *
     * This allows future expansion to synthesis, etc.
     */
    enum class voice_type : uint8_t
    {
        /** Sample-based playback using an audio_sample_t. */
        sample,

        /** Streaming playback using an audio_stream_t. */
        stream,
    };

    /**
     * @brief Single active audio voice.
     *
     * A voice represents one independently controllable sound instance.
     * Voices are owned entirely by the audio engine and live exclusively
     * on the audio thread.
     *
     * A voice is identified externally via a generation-based
     * @ref voice_handle_t, allowing safe reuse of voice slots.
     *
     * Voice lifecycle:
     *  - idle → active (on play)
     *  - active → releasing (on stop or sample end)
     *  - releasing → idle (after envelope completes)
     */
    struct voice_t
    {
        // ---------------------------------------------------------------------
        // Identity / routing
        // ---------------------------------------------------------------------

        /** Current lifecycle state of the voice. */
        voice_state state{ voice_state::idle };

        /** Source type driving this voice. */
        voice_type type{ voice_type::sample };

        /** Target audio bus for mixing. */
        audio_bus_id bus{ audio_bus_id::sfx };

        /** Spatialization mode for this voice. */
        spatial_mode spatial{ spatial_mode::none };

        /**
         * Generation counter used to validate handles.
         *
         * Incremented each time this voice slot is reused to
         * invalidate stale handles.
         */
        uint32_t generation{ 0 };

        /** Public handle identifying this voice instance. */
        voice_handle_t handle{ };

        /** Stereo pan (-1 = left, 0 = center, +1 = right). */
        float pan{ 0.f };

        /** Linear gain applied to this voice. */
        float gain{ 0.2f };

        /** World-space position for spatialized voices. */
        chlm::float3 position{ 0.f };

        /** Maximum audible distance for spatial attenuation. */
        float max_distance{ 50.f };

        /** Reference distance for near-field attenuation. */
        float ref_distance{ 1.f };

        // ---------------------------------------------------------------------
        // Sample-based playback state
        // ---------------------------------------------------------------------

        /** Audio sample backing this voice (if sample-based). */
        const audio_sample_t* sample{ nullptr };

        /**
         * Legacy frame cursor for integer stepping.
         *
         * Currently unused in favor of src_pos/src_step, but kept for now in
         * case debug or tooling still inspect it.
         */
        uint32_t sample_cursor{ 0 };

        /** Whether the voice loops its sample. */
        bool looping{ false };

        /** Loop start frame (inclusive). */
        uint32_t loop_start{ 0 };

        /** Loop end frame (exclusive); 0 means end of sample. */
        uint32_t loop_end{ 0 };

        // ---------------------------------------------------------------------
        // Stream-based playback state
        // ---------------------------------------------------------------------

        /** Audio stream backing this voice (if streaming). */
        audio_stream_t* stream{ nullptr };

        /**
         * Local cache of decoded stream frames pulled from the ring buffer.
         *
         * Interleaved samples: frames * channels.
         * Sized to one fixed stream chunk.
         *
         * Note: streaming data in the ring buffer is already at the engine
         * mix rate (48 kHz).
         */
        float stream_buffer[k_stream_chunk_frames * 2]{ };

        /**
         * Number of valid frames currently stored in stream_buffer.
         *
         * This value is updated when new data is pulled from the stream
         * ring buffer.
         */
        uint32_t stream_frames{ 0 };

        /**
         * Current frame cursor within stream_buffer.
         *
         * Advanced once per frame after all channels are consumed.
         */
        uint32_t stream_frame_cursor{ 0 };

        /**
         * Current channel cursor within the current stream frame.
         *
         * Allows channel-interleaved access without copying.
         */
        uint32_t stream_channel_cursor{ 0 };

        /**
         * Indicates that the voice has exhausted its local stream buffer
         * and is waiting for more decoded data to arrive.
         *
         * While true, the voice does not advance its envelope or playback
         * state, preventing underrun artifacts.
         */
        bool waiting_for_stream{ false };

        // ---------------------------------------------------------------------
        // Playback rate / pitch state (primarily for sample voices)
        // ---------------------------------------------------------------------

        /**
         * Fractional position in source frames.
         *
         * Since all sample assets are converted to the engine mix rate
         * on load (48 kHz), src_pos is in "engine frames" for samples.
         */
        double src_pos{ 0.0 };

        /**
         * Frames advanced per engine frame (1.0 = normal pitch).
         *
         * For sample-based voices at engine rate:
         *   src_step = pitch
         */
        double src_step{ 1.0 };

        /**
         * User-facing pitch control.
         *
         * 1.0 = normal, < 1.0 = slower / lower, > 1.0 = faster / higher.
         * Typically authored via asset pitch + random variance (e.g. footsteps).
         */
        float pitch{ 1.f };

        // ---------------------------------------------------------------------
        // Common playback state
        // ---------------------------------------------------------------------

        /** Whether playback is temporarily paused. */
        bool paused{ false };

        /** Audio frame at which this voice started playback. */
        uint64_t start_frame{ 0 };

        /** Envelope controlling attack, sustain, and release. */
        envelope_t envelope;
    };

    inline uint32_t voice_source_channels(const voice_t& voice) noexcept
    {
        switch (voice.type)
        {
            case voice_type::sample:
                return voice.sample ? voice.sample->channels : 1;
            case voice_type::stream:
                return voice.stream ? voice.stream->channels : 1;
        }

        return 1;
    }

    // -------------------------------------------------------------------------
    // Sample-based playback helpers (48 kHz internal, with per-voice pitch)
    // -------------------------------------------------------------------------

    inline bool sample_voice_next_frame_linear(voice_t& voice, float& out_l, float& out_r) noexcept
    {
        out_l = 0.f;
        out_r = 0.f;

        const auto* s{ voice.sample };
        if (!s) return false;

        const uint32_t total_frames{ s->frame_count };
        const uint32_t channels{ s->channels };

        if (total_frames == 0 || channels == 0)
            return false;

        // Compute logical end of playback for this voice
        const uint32_t loop_start{ voice.loop_start };
        uint32_t loop_end{ voice.loop_end };

        if (!voice.looping || loop_end == 0 || loop_end > total_frames || loop_end <= loop_start)
            loop_end = total_frames;

        const uint32_t sample_end{ loop_end };

        // Early-out end-of-sample handling for non-looping voices
        if (!voice.looping && voice.src_pos >= static_cast<double>(sample_end))
        {
            voice.state = voice_state::releasing;
            return false;
        }

        resample_request_t req{ };
        req.data = s->data;
        req.total_frames = total_frames;
        req.channels = channels;
        req.src_pos = voice.src_pos;
        req.src_step = voice.src_step;
        req.looping = voice.looping;
        req.loop.start = loop_start;
        req.loop.end = loop_end;

        const bool ok{ resample_linear_frame(req, out_l, out_r) };

        // Propagate updated position back into the voice.
        voice.src_pos = req.src_pos;

        if (!ok)
        {
            if (!voice.looping)
                voice.state = voice_state::releasing;

            return false;
        }

        // Non-looping: once we’ve stepped past the end, enter release.
        if (!voice.looping && voice.src_pos >= static_cast<double>(sample_end))
        {
            voice.state = voice_state::releasing;
        }

        return true;
    }

    /**
     * @brief Produces the next mono sample for a voice.
     *
     * For sample voices:
     *  - Advances playback via linear interpolation at the voice's
     *    current playback rate (pitch).
     *  - Returns the left channel (mono assets are duplicated to L/R).
     *
     * For streaming voices:
     *  - Consumes interleaved frames from the stream buffer,
     *    which is already at engine sample rate (48 kHz).
     *
     * No mixing, spatialization, or envelope processing occurs here.
     *
     * @param voice Voice instance to advance
     * @return Next sample value, or 0.0f if silent
     *
     * @note
     * This function is real-time safe and must not allocate,
     * lock, or perform unbounded work.
     */
    inline float voice_next_sample(voice_t& voice) noexcept
    {
        switch (voice.type)
        {
            case voice_type::sample:
            {
                if (!voice.sample) return 0.f;
                if (voice.paused) return 0.f;

                float l{ 0.f };
                float r{ 0.f };

                if (!sample_voice_next_frame_linear(voice, l, r))
                    return 0.f;

                // Mono path: just return left (mono assets are already duplicated)
                return l;
            }

            case voice_type::stream:
            {
                if (!voice.stream) return 0.f;

                if (voice.paused)
                    return 0.f;

                if (voice.stream_frame_cursor >= voice.stream_frames)
                {
                    voice.stream_frames = voice.stream->buffer.read(voice.stream_buffer, k_stream_chunk_frames);
                    voice.stream_frame_cursor = 0;

                    if (voice.stream_frames == 0)
                    {
                        const bool eof{ voice.stream->eof.load(std::memory_order_acquire) };

                        if (eof && !voice.stream->looping)
                        {
                            voice.waiting_for_stream = false;
                            voice.state = voice_state::idle;

                            return 0.f;
                        }

                        voice.waiting_for_stream = true;
                        return 0.f;
                    }

                    voice.waiting_for_stream = false;
                }

                // Always read left channel (channel 0) — for now
                // Will later make this channel-aware when we render per-channel
                const uint32_t idx{ voice.stream_frame_cursor * voice.stream->channels + 0 };

                const float sample{ voice.stream_buffer[idx] };

                // === Advance the full frame now (critical) ===
                voice.stream_frame_cursor++;
                voice.stream_channel_cursor = 0; // just housekeeping, can remove later

                // LOG_AUDIO_INFO("Stream: {} Hz, Engine: {} Hz  cursor={}/{}", voice.stream->sample_rate, sample_rate,
                //                voice.stream_frame_cursor, voice.stream_frames);

                return sample;
            }
        }

        return 0.f;
    }

    /**
     * @brief Produces the next stereo frame for a voice (L/R).
     *
     * For sample voices:
     *  - Uses the shared resampling helper to advance playback at the
     *    current voice pitch (src_step).
     *
     * For streaming voices:
     *  - Consumes interleaved frames from the stream buffer,
     *    which is already at engine sample rate (48 kHz).
     *
     * No spatialization or envelope processing occurs here.
     *
     * @param voice Voice instance to advance
     * @param out_l [out] left sample.
     * @param out_r [out] right sample.
     *
     * @note
     * This function is real-time safe and must not allocate,
     * lock, or perform unbounded work.
     */
    inline void voice_next_stereo_frame(voice_t& voice, float& out_l, float& out_r) noexcept
    {
        out_l = 0.f;
        out_r = 0.f;

        switch (voice.type)
        {
            case voice_type::sample:
            {
                if (!voice.sample) return;

                CE_ASSERT(voice.sample && voice.sample->channels == 2,
                          "voice_next_stereo_frame called on non-stereo sample voice");

                if (voice.paused) return;

                // Shared helper: handles mono or stereo sample assets.
                if (!sample_voice_next_frame_linear(voice, out_l, out_r))
                    return;

                return;
            }

            case voice_type::stream:
            {
                if (!voice.stream) return;

                CE_ASSERT(voice.stream && voice.stream->channels == 2,
                          "voice_next_stereo_frame called on non-stereo stream voice");

                if (voice.paused)
                    return; // out_l/out_r already 0

                if (voice.stream_frame_cursor >= voice.stream_frames)
                {
                    voice.stream_frames = voice.stream->buffer.read(voice.stream_buffer, k_stream_chunk_frames);
                    voice.stream_frame_cursor = 0;

                    if (voice.stream_frames == 0)
                    {
                        const bool eof{ voice.stream->eof.load(std::memory_order_acquire) };

                        if (eof && !voice.stream->looping)
                        {
                            voice.waiting_for_stream = false;
                            voice.state = voice_state::idle;

                            return; // out_l/out_r already 0
                        }

                        voice.waiting_for_stream = true;
                        return;
                    }

                    voice.waiting_for_stream = false;
                }

                const uint32_t base{
                    voice.stream_frame_cursor * voice.stream->channels
                };

                out_l = voice.stream_buffer[base + 0];
                out_r = voice.stream_buffer[base + 1];

                // Advance full frame
                voice.stream_frame_cursor++;
                voice.stream_channel_cursor = 0; // still housekeeping
            }
        }
    }
} // namespace carrot::audio
