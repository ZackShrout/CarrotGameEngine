//
// Created by Zack Shrout on 2/17/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Audio/Sample/WavCore.h"

#include <atomic>
#include <thread>

namespace carrot::audio {
    struct audio_stream_t;

    class wav_stream_decoder_t
    {
    public:
        ~wav_stream_decoder_t() { stop(); }

        bool open(std::string_view path, audio_stream_t* stream) noexcept;
        void start() noexcept;
        void stop() noexcept;

    private:
        void thread_main() noexcept;

        std::thread _thread;
        std::atomic<bool> _running{ false };

        FILE* _file{ nullptr };
        audio_stream_t* _stream{ nullptr };

        fmt_chunk_t _fmt{ };
        uint32_t _data_bytes_remaining{ 0 };
    };
} // namespace carrot::audio
