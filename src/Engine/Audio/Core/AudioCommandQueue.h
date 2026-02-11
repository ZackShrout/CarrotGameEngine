//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "AudioCommand.h"

#include <atomic>
#include <cstdint>

namespace carrot::audio {
    template<uint32_t Capacity>
    class audio_command_queue_t
    {
    public:
        audio_command_queue_t() = default;

        bool push(const audio_command_t& cmd) noexcept;
        bool pop(audio_command_t& out_cmd) noexcept;

    private:
        audio_command_t _buffer[Capacity]{ };

        std::atomic<uint32_t> _write{ 0 };
        std::atomic<uint32_t> _read{ 0 };
    };

    template<uint32_t Capacity>
    bool audio_command_queue_t<Capacity>::push(const audio_command_t& cmd) noexcept
    {
        const uint32_t write{ _write.load(std::memory_order_relaxed) };
        const uint32_t next{ (write + 1) % Capacity };

        if (next == _read.load(std::memory_order_acquire))
            return false; // queue full

        _buffer[write] = cmd;
        _write.store(next, std::memory_order_release);
        return true;
    }

    template<uint32_t Capacity>
    bool audio_command_queue_t<Capacity>::pop(audio_command_t& out_cmd) noexcept
    {
        const uint32_t read{ _read.load(std::memory_order_relaxed) };

        if (read == _write.load(std::memory_order_acquire))
            return false; // empty

        out_cmd = _buffer[read];
        _read.store((read + 1) % Capacity, std::memory_order_release);
        return true;
    }
} // namespace carrot::audio
