//
// Created by Zack Shrout on 2/17/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Audio/Sample/WavCore.h"

#include <atomic>
#include <thread>

namespace carrot::audio {
#if defined(_WIN32)
    using carrot_offset_t = __int64;
#else
    using carrot_offset_t = off_t; // typically 64-bit with _FILE_OFFSET_BITS=64
#endif

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
        void enter_loop_phase() noexcept;

        std::thread _thread;
        std::atomic<bool> _running{ false };

        FILE* _file{ nullptr };
        audio_stream_t* _stream{ nullptr };

        fmt_chunk_t _fmt{ };
        uint64_t _data_bytes_remaining{ 0 };
        uint64_t _data_bytes_total{ 0 };
        carrot_offset_t _data_start_offset{ 0 };

        carrot_offset_t _loop_start_offset{ 0 };
        carrot_offset_t _loop_end_offset{ 0 }; // one past last byte of loop region
        bool _use_loop_region{ false };
        bool _in_loop_phase{ false };
    };
} // namespace carrot::audio
