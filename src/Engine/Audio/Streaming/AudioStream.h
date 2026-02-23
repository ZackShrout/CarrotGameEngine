//
// Created by Zack Shrout on 2/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "WavStreamDecoder.h"
#include "AudioRingBuffer.h"
#include "Audio/Core/AudioCore.h"

#include <atomic>

#include "Audio/Voice/VoiceHandle.h"

namespace carrot::audio {
    /**
     * @brief Incrementally decoded audio stream.
     *
     * audio_stream_t represents a long-lived audio source whose PCM data
     * is produced asynchronously (disk / decoder thread) and consumed
     * by the real-time audio thread via a lock-free ring buffer.
     *
     * Lifetime rules:
     *  - Created on the engine thread
     *  - Filled on a non-RT decode thread
     *  - Read-only from the audio thread
     *  - Destroyed only after all referencing voices have finished
     */
    struct audio_stream_t
    {
        audio_ring_buffer_t<k_buffer_frames, k_max_channels> buffer;

        uint32_t channels{ 0 };
        uint32_t sample_rate{ 0 };

        wav_stream_decoder_t decoder;

        bool active{ false };
        bool looping{ false };

        /** Loop start frame (inclusive). */
        uint64_t loop_start{ 0 };

        /** Loop end frame (exclusive); 0 means end of sample. */
        uint64_t loop_end{ 0 };

        voice_handle_t owning_voice{ voice_handle_t::invalid() };

        /** True once the decoder has reached end-of-stream */
        std::atomic<bool> eof{ false };
    };

    inline void init_audio_stream(audio_stream_t& stream, const uint32_t channels, const uint32_t sample_rate) noexcept
    {
        stream.channels = channels;
        stream.sample_rate = sample_rate;
        stream.eof.store(false, std::memory_order_relaxed);

        stream.buffer.init(channels);
    }
} // namespace carrot::audio
