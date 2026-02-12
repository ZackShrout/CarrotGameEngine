//
// Created by Zack Shrout on 2/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "WavLoader.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace carrot::audio {
    namespace {
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

        bool id_equals(const char id[4], const char* str)
        {
            return std::memcmp(id, str, 4) == 0;
        }

        float pcm24_to_float(const uint8_t* p)
        {
            // Assemble signed 24-bit little-endian
            int32_t v{ p[0] | p[1] << 8 | p[2] << 16 };

            // Sign extend
            if (v & 0x00800000)
                v |= 0xFF000000;

            // Normalize
            return static_cast<float>(v) / 8388608.0f;
        }
    } // anonymous namespace

    audio_sample_t* load_wav_file(std::string_view path)
    {
        FILE* file{ std::fopen(path.data(), "rb") };

        if (!file) return nullptr;

        riff_header_t riff{ };
        std::fread(&riff, sizeof(riff), 1, file);

        if (!id_equals(riff.id, "RIFF") || !id_equals(riff.format, "WAVE"))
        {
            std::fclose(file);
            return nullptr;
        }

        fmt_chunk_t fmt{ };
        uint8_t* pcm_data{ nullptr };
        uint32_t pcm_size{ 0 };

        while (!std::feof(file))
        {
            chunk_header_t chunk{ };

            if (std::fread(&chunk, sizeof(chunk), 1, file) != 1)
                break;

            if (id_equals(chunk.id, "fmt "))
            {
                std::fread(&fmt, sizeof(fmt), 1, file);
                std::fseek(file, static_cast<int32_t>(chunk.size - sizeof(fmt)), SEEK_CUR);
            }
            else if (id_equals(chunk.id, "data"))
            {
                pcm_size = chunk.size;
                pcm_data = static_cast<uint8_t *>(std::malloc(pcm_size));
                std::fread(pcm_data, pcm_size, 1, file);
            }
            else
            {
                std::fseek(file, chunk.size, SEEK_CUR);
            }
        }

        std::fclose(file);

        if (!pcm_data || (fmt.audio_format != 1 && fmt.audio_format != 3))
        {
            std::free(pcm_data);
            return nullptr;
        }

        const uint32_t frame_count{ pcm_size / (fmt.num_channels * (fmt.bits_per_sample / 8)) };
        float* samples{ static_cast<float *>(std::malloc(sizeof(float) * frame_count * fmt.num_channels)) };

        if (fmt.audio_format == 1 && fmt.bits_per_sample == 16)
        {
            const int16_t* src = reinterpret_cast<int16_t *>(pcm_data);
            for (uint32_t i = 0; i < frame_count * fmt.num_channels; ++i)
                samples[i] = static_cast<float>(src[i]) / 32768.0f;
        }
        else if (fmt.audio_format == 1 && fmt.bits_per_sample == 24)
        {
            const uint8_t* src = pcm_data;
            float* dst = samples;

            const uint32_t total_samples = frame_count * fmt.num_channels;

            for (uint32_t i = 0; i < total_samples; ++i)
            {
                *dst++ = pcm24_to_float(src);
                src += 3;
            }
        }
        else if (fmt.audio_format == 3 && fmt.bits_per_sample == 32)
        {
            std::memcpy(samples, pcm_data, pcm_size);
        }
        else
        {
            std::free(samples);
            std::free(pcm_data);
            return nullptr;
        }

        std::free(pcm_data);

        audio_sample_t* sample{ new audio_sample_t{ } };
        sample->data = samples;
        sample->frame_count = frame_count;
        sample->channels = fmt.num_channels;
        sample->sample_rate = fmt.sample_rate;

        return sample;
    }
} // namespace carrot::audio
