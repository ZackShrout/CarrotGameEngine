//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "AudioCommand.h"

#include <atomic>
#include <cstdint>

namespace carrot::audio {
    /**
     * @brief Lock-free single-producer, single-consumer audio command queue.
     *
     * This queue is used to send commands from the engine or game thread
     * to the real-time audio thread.
     *
     * Design assumptions:
     *  - Single producer (engine / game thread)
     *  - Single consumer (audio thread)
     *  - Fixed capacity
     *  - No dynamic memory allocation
     *
     * Commands must be trivially copyable and safe to consume from
     * a real-time context.
     *
     * If the queue is full, commands are dropped and false is returned.
     * This prevents blocking or allocation on the producer thread.
     *
     * @tparam Capacity Maximum number of queued commands
     */
    template<uint32_t capacity>
    class audio_command_queue_t
    {
    public:
        audio_command_queue_t() = default;

        /**
         * @brief Pushes a command into the queue.
         *
         * This function is safe to call from the engine or game thread.
         * If the queue is full, the command is dropped.
         *
         * @param cmd Command to enqueue
         * @return True if the command was enqueued, false if the queue was full
         */
        bool push(const audio_command_t& cmd) noexcept;

        /**
         * @brief Pops the next command from the queue.
         *
         * This function is intended to be called from the real-time
         * audio thread.
         *
         * @param out_cmd Destination for the popped command
         * @return True if a command was popped, false if the queue was empty
         */
        bool pop(audio_command_t& out_cmd) noexcept;

    private:
        /** Circular buffer storing queued audio commands. */
        audio_command_t _buffer[capacity]{ };

        /** Write index (engine / game thread only). */
        std::atomic<uint32_t> _write{ 0 };

        /** Read index (audio thread only). */
        std::atomic<uint32_t> _read{ 0 };
    };

    template<uint32_t Capacity>
    bool audio_command_queue_t<Capacity>::push(const audio_command_t& cmd) noexcept
    {
        const uint32_t write{ _write.load(std::memory_order_relaxed) };
        const uint32_t next{ (write + 1) % Capacity };

        if (next == _read.load(std::memory_order_acquire))
            return false; // queue full - drop command to preserve non-blocking behavior

        _buffer[write] = cmd;
        _write.store(next, std::memory_order_release);
        return true;
    }

    template<uint32_t Capacity>
    bool audio_command_queue_t<Capacity>::pop(audio_command_t& out_cmd) noexcept
    {
        const uint32_t read{ _read.load(std::memory_order_relaxed) };

        if (read == _write.load(std::memory_order_acquire))
            return false; // queue empty

        out_cmd = _buffer[read];
        _read.store((read + 1) % Capacity, std::memory_order_release);
        return true;
    }
} // namespace carrot::audio
