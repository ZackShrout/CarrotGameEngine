//
// Created by Zack Shrout on 2/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>
#include <cstdlib>

#include "../../Core/CoreDefines.h"

namespace carrot::audio {
    /**
     * @brief Immutable decoded audio sample.
     *
     * audio_sample_t represents fully decoded PCM audio data stored
     * in interleaved floating-point format.
     *
     * Samples are created and owned on the engine or asset-loading thread
     * and are referenced by audio voices on the real-time audio thread.
     *
     * @note
     * audio_sample_t objects:
     *  - are immutable after creation
     *  - perform no allocation or deallocation on the audio thread
     *  - may be referenced by multiple voices concurrently
     *  - must outlive any voice that references them
     *
     * Destruction must only occur when it is guaranteed that no
     * audio thread voice is accessing the sample.
     */
    struct audio_sample_t
    {
        audio_sample_t() = default;

        /**
         * @brief Destroys the audio sample and releases its PCM buffer.
         *
         * @warning
         * This destructor must never be called while the audio thread
         * may still reference this sample.
         */
        ~audio_sample_t() { std::free(data); data = nullptr; }

        DISABLE_COPY(audio_sample_t)

        /** Interleaved PCM sample data (float32). */
        float* data{ nullptr };

        /** Number of audio frames in the sample. */
        uint32_t frame_count{ };

        /** Number of channels per frame. */
        uint32_t channels{ };

        /** Sample rate of the decoded audio (Hz). */
        uint32_t sample_rate{ };
    };
} // namespace carrot::audio
