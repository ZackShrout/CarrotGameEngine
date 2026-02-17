//
// Created by Zack Shrout on 2/17/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Audio/Streaming/AudioStream.h"

#include <atomic>
#include <thread>

namespace carrot::audio {

    /**
     * @brief Simple test-only streaming decoder.
     *
     * Continuously writes a generated waveform into an audio_stream_t
     * to validate streaming infrastructure.
     *
     * This exists purely for development and should be removed once
     * real file-based streaming is implemented.
     */
    class test_stream_decoder_t
    {
    public:
        test_stream_decoder_t() = default;
        ~test_stream_decoder_t();

        void start(audio_stream_t* stream) noexcept;
        void stop() noexcept;

    private:
        void thread_main() noexcept;

        audio_stream_t* _stream{nullptr};
        std::thread _thread;
        std::atomic<bool> _running{false};
    };

} // namespace carrot::audio
