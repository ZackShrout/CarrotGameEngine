//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Audio/DSP/Envelope.h"
#include "Audio/Mixer/AudioBus.h"
#include "Audio/Sample/AudioSample.h"
#include "Audio/Core/AudioCore.h"

#include <chlm/Core.h>

#include "VoiceHandle.h"
#include "Audio/Streaming/AudioStream.h"

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

        /** Current playback cursor in sample frames. */
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
        // Common playback state
        // ---------------------------------------------------------------------

        /** Whether playback is temporarily paused. */
        bool paused{ false };

        /** Audio frame at which this voice started playback. */
        uint64_t start_frame{ 0 };

        /** Envelope controlling attack, sustain, and release. */
        envelope_t envelope;
    };

    /**
     * @brief Produces the next raw sample for a voice.
     *
     * This function advances the voice playback cursor and handles
     * looping and end-of-sample behavior. It performs no mixing,
     * spatialization, or envelope processing.
     *
     * @param voice Voice instance to advance
     * @param sample_rate Output sample rate (currently unused)
     * @return Next sample value, or 0.0f if silent
     *
     * @note
     * This function is real-time safe and must not allocate,
     * lock, or perform unbounded work.
     */
    inline float voice_next_sample(voice_t& voice, [[maybe_unused]] const double sample_rate) noexcept
    {
        switch (voice.type)
        {
            case voice_type::sample:
            {
                if (voice.paused)
                    return 0.0f;

                const uint32_t sample_end{
                    voice.looping && voice.loop_end > 0 ? voice.loop_end : voice.sample->frame_count
                };

                if (voice.sample_cursor >= sample_end)
                {
                    if (voice.looping)
                    {
                        voice.sample_cursor = voice.loop_start;
                    }
                    else
                    {
                        voice.state = voice_state::releasing;
                        return 0.0f;
                    }
                }

                const uint32_t idx{ voice.sample_cursor++ * voice.sample->channels };

                return voice.sample->data[idx];
            }

            case voice_type::stream:
            {
                if (voice.stream_frame_cursor >= voice.stream_frames)
                    return 0.0f;

                // Always read left channel (channel 0) — for now
                // Will later make this channel-aware when we render per-channel
                const uint32_t idx{ voice.stream_frame_cursor * voice.stream->channels + 0 };

                const float sample{ voice.stream_buffer[idx] };

                // === Advance the full frame now (critical) ===
                voice.stream_frame_cursor++;
                voice.stream_channel_cursor = 0;  // just housekeeping, can remove later

                // LOG_AUDIO_INFO("Stream: {} Hz, Engine: {} Hz  cursor={}/{}", voice.stream->sample_rate, sample_rate,
                //                voice.stream_frame_cursor, voice.stream_frames);

                return sample;
            }
        }

        return 0.0f;
    }
} // namespace carrot::audio
