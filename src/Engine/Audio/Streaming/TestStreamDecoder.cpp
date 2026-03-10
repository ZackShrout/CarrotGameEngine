//
// Created by Zack Shrout on 2/17/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "TestStreamDecoder.h"

#include "Audio/Core/AudioCore.h"

#include <cmath>
#include <chrono>
#include <thread>

namespace carrot::audio {
    test_stream_decoder_t::~test_stream_decoder_t()
    {
        stop();
    }

    void test_stream_decoder_t::start(audio_stream_t* stream) noexcept
    {
        if (_running.load(std::memory_order_relaxed))
            return;

        _stream = stream;
        _running.store(true, std::memory_order_release);

        _thread = std::thread(&test_stream_decoder_t::thread_main, this);
    }

    void test_stream_decoder_t::stop() noexcept
    {
        if (!_running.exchange(false, std::memory_order_acq_rel))
            return;

        if (_thread.joinable())
            _thread.join();

        if (_stream)
            _stream->eof.store(true, std::memory_order_release);
    }

    void test_stream_decoder_t::thread_main() noexcept
    {
        constexpr uint32_t frames_per_chunk{ 256 };
        constexpr float frequency{ 440.f };

        float phase{ 0.f };
        const float phase_inc{ 2.0f * 3.14159265359f * frequency / static_cast<float>(_stream->sample_rate) };
        float temp[frames_per_chunk * k_max_channels]{ };

        while (_running.load(std::memory_order_acquire))
        {
            const uint32_t writable{ _stream->buffer.available_write() };

            const uint32_t frames_to_write{ chlm::min(writable, frames_per_chunk) };

            if (frames_to_write == 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            for (uint32_t i{ 0 }; i < frames_to_write; ++i)
            {
                const float s{ chlm::sin(phase) };

                static uint32_t dbg{ 0 };
                if ((dbg++ & 0x3FFF) == 0)
                {
                    LOG_AUDIO_INFO("[DECODER] phase={}, sample={}", phase, s);
                }

                phase += phase_inc;
                if (phase >= 2.0f * chlm::pi)
                    phase -= 2.0f * chlm::pi;

                for (uint32_t ch = 0; ch < _stream->channels; ++ch)
                    temp[i * _stream->channels + ch] = s;
            }

            _stream->buffer.write(temp, frames_to_write);

            static auto last{ std::chrono::steady_clock::now() };

            auto now{ std::chrono::steady_clock::now() };
            if (now - last > std::chrono::seconds(1))
            {
                last = now;

                LOG_AUDIO_INFO(
                    "[DECODER] writable={}, total_written={}",
                    _stream->buffer.available_write(),
                    _stream->buffer.debug.total_written_frames.load()
                );
            }
        }
    }
} // namespace carrot::audio
