//
// Created by Zack Shrout on 2/16/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>
#include <atomic>

namespace carrot::audio {
    template<uint32_t capacity_frames, uint32_t max_channels>
    class audio_ring_buffer_t
    {
    public:
        void init(const uint32_t channels) noexcept
        {
            CE_ASSERT(channels > 0 && channels <= max_channels);

            _channels = channels;
            _write.store(0, std::memory_order_relaxed);
            _read.store(0, std::memory_order_relaxed);
        }

        // Decode thread
        uint32_t write(const float* src, uint32_t frames) noexcept;

        // Audio thread
        uint32_t read(float* dst, uint32_t frames) noexcept;

        uint32_t available_read() const noexcept;
        uint32_t available_write() const noexcept;

    private:
        static uint32_t advance(uint32_t pos, uint32_t frames) noexcept;
        static uint32_t distance(uint32_t from, uint32_t to) noexcept;
        void write_frames(uint32_t write_pos, const float* src, uint32_t frames) noexcept;
        void read_frames(uint32_t read_pos, float* dst, uint32_t frames) noexcept;

        alignas(64) float _data[capacity_frames * max_channels]{ };

        uint32_t _channels{ 0 };

        std::atomic<uint32_t> _write;
        std::atomic<uint32_t> _read;
    };

    // PUBLIC

    template<uint32_t capacity_frames, uint32_t MaxChannels>
    uint32_t audio_ring_buffer_t<capacity_frames, MaxChannels>::write(const float* src, const uint32_t frames) noexcept
    {
        const uint32_t write_pos{ _write.load(std::memory_order_relaxed) };
        const uint32_t read_pos{ _read.load(std::memory_order_acquire) };

        const uint32_t free_frames{ capacity_frames - distance(read_pos, write_pos) };

        const uint32_t to_write{ frames < free_frames ? frames : free_frames };
        if (to_write == 0)
            return 0;

        write_frames(write_pos, src, to_write);

        _write.store(advance(write_pos, to_write), std::memory_order_release);

        return to_write;
    }

    template<uint32_t capacity_frames, uint32_t max_channels>
    uint32_t audio_ring_buffer_t<capacity_frames, max_channels>::read(float* dst, const uint32_t frames) noexcept
    {
        const uint32_t read_pos{ _read.load(std::memory_order_relaxed) };
        const uint32_t write_pos{ _write.load(std::memory_order_acquire) };

        const uint32_t available{ distance(read_pos, write_pos) };

        const uint32_t to_read{ frames < available ? frames : available };
        if (to_read == 0)
            return 0;

        read_frames(read_pos, dst, to_read);

        _read.store(advance(read_pos, to_read), std::memory_order_release);

        return to_read;
    }

    template<uint32_t capacity_frames, uint32_t max_channels>
    uint32_t audio_ring_buffer_t<capacity_frames, max_channels>::available_read() const noexcept
    {
        const uint32_t r{ _read.load(std::memory_order_acquire) };
        const uint32_t w{ _write.load(std::memory_order_acquire) };

        return distance(r, w);
    }

    template<uint32_t capacity_frames, uint32_t max_channels>
    uint32_t audio_ring_buffer_t<capacity_frames, max_channels>::available_write() const noexcept
    {
        return capacity_frames - available_read();
    }

    // PRIVATE

    template<uint32_t capacity_frames, uint32_t max_channels>
    uint32_t audio_ring_buffer_t<capacity_frames, max_channels>::advance(uint32_t pos, const uint32_t frames) noexcept
    {
        pos += frames;
        if (pos >= capacity_frames)
            pos -= capacity_frames;

        return pos;
    }

    template<uint32_t capacity_frames, uint32_t max_channels>
    uint32_t audio_ring_buffer_t<capacity_frames, max_channels>::distance(
        const uint32_t from, const uint32_t to) noexcept
    {
        return to >= from ? to - from : capacity_frames - from + to;
    }

    template<uint32_t capacity_frames, uint32_t max_channels>
    void audio_ring_buffer_t<capacity_frames, max_channels>::write_frames(const uint32_t write_pos, const float* src,
        const uint32_t frames) noexcept
    {
        const uint32_t first{ std::min(frames, capacity_frames - write_pos) };
        const uint32_t samples1{ first * _channels };

        std::memcpy(&_data[write_pos * _channels], src, samples1 * sizeof(float));

        if (first < frames)
        {
            const uint32_t samples2{ (frames - first) * _channels };

            std::memcpy(&_data[0], src + samples1, samples2 * sizeof(float));
        }
    }

    template<uint32_t capacity_frames, uint32_t max_channels>
    void audio_ring_buffer_t<capacity_frames, max_channels>::read_frames(const uint32_t read_pos, float* dst,
        const uint32_t frames) noexcept
    {
        const uint32_t first{ std::min(frames, capacity_frames - read_pos) };

        const uint32_t samples1 = first * _channels;
        std::memcpy(dst, &_data[read_pos * _channels], samples1 * sizeof(float));

        if (first < frames)
        {
            const uint32_t samples2{ (frames - first) * _channels };

            std::memcpy(dst + samples1, &_data[0], samples2 * sizeof(float));
        }
    }
} // namespace carrot::audio
