//
// Created by Zack Shrout on 2/17/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>
#include <cstring>

namespace carrot::audio {
    struct riff_header_t
    {
        char id[4]; // "RIFF"
        uint32_t size;
        char format[4]; // "WAVE"
    };

    struct chunk_header_t
    {
        char id[4];
        uint32_t size;
    };

    struct fmt_chunk_t
    {
        uint16_t audio_format; // 1 = PCM, 3 = IEEE float
        uint16_t num_channels;
        uint32_t sample_rate;
        uint32_t byte_rate;
        uint16_t block_align;
        uint16_t bits_per_sample;
    };

    inline bool id_equals(const char id[4], const char* str)
    {
        return std::memcmp(id, str, 4) == 0;
    }

    inline float pcm24_to_float(const uint8_t* p)
    {
        // Assemble signed 24-bit little-endian
        int32_t v{ p[0] | p[1] << 8 | p[2] << 16 };

        // Sign extend
        if (v & 0x00800000)
            v |= static_cast<int>(0xFF000000);

        // Normalize
        return static_cast<float>(v) / 8388608.0f;
    }
} // namespace carrot::audio
