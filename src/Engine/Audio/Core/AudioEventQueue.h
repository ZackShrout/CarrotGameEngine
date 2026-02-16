//
// Created by Zack Shrout on 2/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "AudioEvent.h"

#include <atomic>
#include <cstdint>

namespace carrot::audio {
    template<uint32_t capacity>
    class audio_event_queue_t
    {
    public:
        bool push(const audio_event_t& evt) noexcept;
        bool pop(audio_event_t& out_evt) noexcept;

    private:
        audio_event_t _buffer[capacity]{};

        std::atomic<uint32_t> _write{0};
        std::atomic<uint32_t> _read{0};
    };

    template<uint32_t Capacity>
    bool audio_event_queue_t<Capacity>::push(const audio_event_t& evt) noexcept
    {
        const uint32_t write{ _write.load(std::memory_order_relaxed) };
        const uint32_t next{ (write + 1) % Capacity };

        if (next == _read.load(std::memory_order_acquire))
            return false; // queue full — drop event (acceptable)

        _buffer[write] = evt;
        _write.store(next, std::memory_order_release);
        return true;
    }

    template<uint32_t Capacity>
    bool audio_event_queue_t<Capacity>::pop(audio_event_t& out_evt) noexcept
    {
        const uint32_t read{ _read.load(std::memory_order_relaxed) };

        if (read == _write.load(std::memory_order_acquire))
            return false; // empty

        out_evt = _buffer[read];
        _read.store((read + 1) % Capacity, std::memory_order_release);
        return true;
    }
} // namespace carrot::audio