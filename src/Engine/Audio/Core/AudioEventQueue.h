//
// Created by Zack Shrout on 2/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "AudioEvent.h"

#include <atomic>
#include <cstdint>

namespace carrot::audio {
    /**
     * @brief Lock-free single-producer, single-consumer audio event queue.
     *
     * This queue is used to send events from the real-time audio thread
     * to the engine or game thread without blocking or allocation.
     *
     * Design assumptions:
     *  - Single producer (audio thread)
     *  - Single consumer (engine thread)
     *  - Fixed capacity
     *  - No dynamic memory allocation
     *
     * If the queue is full, events are dropped intentionally to preserve
     * real-time safety.
     *
     * @tparam Capacity Maximum number of queued events
     */
    template<uint32_t capacity>
    class audio_event_queue_t
    {
    public:
        /**
         * @brief Pushes an audio event into the queue.
         *
         * This function is safe to call from the real-time audio thread.
         * If the queue is full, the event is dropped and false is returned.
         *
         * @param evt Event to enqueue
         * @return True if the event was enqueued, false if the queue was full
         */
        bool push(const audio_event_t& evt) noexcept;

        /**
         * @brief Pops the next audio event from the queue.
         *
         * This function is intended to be called from the engine or game thread.
         *
         * @param out_evt Destination for the popped event
         * @return True if an event was popped, false if the queue was empty
         */
        bool pop(audio_event_t& out_evt) noexcept;

    private:
        /** Circular buffer storing audio events. */
        audio_event_t _buffer[capacity]{};

        /** Write index (audio thread only). */
        std::atomic<uint32_t> _write{0};

        /** Read index (engine thread only). */
        std::atomic<uint32_t> _read{0};
    };

    template<uint32_t Capacity>
    bool audio_event_queue_t<Capacity>::push(const audio_event_t& evt) noexcept
    {
        const uint32_t write{ _write.load(std::memory_order_relaxed) };
        const uint32_t next{ (write + 1) % Capacity };

        if (next == _read.load(std::memory_order_acquire))
            return false; // queue full — event dropped to preserve RT safety

        _buffer[write] = evt;
        _write.store(next, std::memory_order_release);
        return true;
    }

    template<uint32_t Capacity>
    bool audio_event_queue_t<Capacity>::pop(audio_event_t& out_evt) noexcept
    {
        const uint32_t read{ _read.load(std::memory_order_relaxed) };

        if (read == _write.load(std::memory_order_acquire))
            return false; // queue empty

        out_evt = _buffer[read];
        _read.store((read + 1) % Capacity, std::memory_order_release);
        return true;
    }
} // namespace carrot::audio
