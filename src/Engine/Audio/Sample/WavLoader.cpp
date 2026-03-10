//
// Created by Zack Shrout on 2/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "WavLoader.h"

#include "WavCore.h"
#include "Audio/Core/AudioCore.h"
#include "Audio/DSP/Resampler.h"

#include <cstdio>

namespace carrot::audio {
    audio_sample_t* load_wav_file(const std::string_view path)
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
                std::fseek(file, static_cast<int32_t>(chunk.size), SEEK_CUR);
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
            const int16_t* src{ reinterpret_cast<int16_t *>(pcm_data) };
            for (uint32_t i{ 0 }; i < frame_count * fmt.num_channels; ++i)
                samples[i] = static_cast<float>(src[i]) / 32768.f;
        }
        else if (fmt.audio_format == 1 && fmt.bits_per_sample == 24)
        {
            const uint8_t* src{ pcm_data };
            float* dst{ samples };
            const uint32_t total_samples{ frame_count * fmt.num_channels };

            for (uint32_t i{ 0 }; i < total_samples; ++i)
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

        // ──────────────────────────────────────────────────────────────────
        // Offline resample to engine mix rate (48k) for sample-based assets
        // ──────────────────────────────────────────────────────────────────

        if (sample->sample_rate != k_engine_sample_rate && sample->frame_count > 0 && sample->channels > 0
            && sample->data)
        {
            const double src_rate{ static_cast<double>(sample->sample_rate) };
            constexpr double dst_rate{ static_cast<double>(k_engine_sample_rate) };

            const uint32_t src_frames{ sample->frame_count };
            const uint32_t channels{ sample->channels };

            const double frame_ratio{ dst_rate / src_rate };
            const uint32_t dst_frames{
                static_cast<uint32_t>(std::ceil(static_cast<double>(src_frames) * frame_ratio))
            };

            float* dst_data{ static_cast<float *>(std::malloc(sizeof(float) * dst_frames * channels)) };

            if (!dst_data)
            {
                // Allocation failed; fall back to original sample
                return sample;
            }

            resample_request_t req{ };
            req.data = sample->data;
            req.total_frames = src_frames;
            req.channels = channels;
            req.src_pos = 0.0;
            req.src_step = src_rate / dst_rate; // inverse of runtime direction
            req.looping = false;
            req.loop.start = 0;
            req.loop.end = src_frames;

            for (uint32_t i{ 0 }; i < dst_frames; ++i)
            {
                float l{ 0.f };
                float r{ 0.f };

                if (!resample_linear_frame(req, l, r))
                {
                    // Ran out of source; pad remainder with zeros.
                    for (uint32_t ch{ 0 }; ch < channels; ++ch)
                    {
                        dst_data[static_cast<size_t>(i) * channels + ch] = 0.f;
                    }
                    continue;
                }

                if (channels == 1)
                {
                    dst_data[static_cast<size_t>(i) * channels + 0] = l;
                }
                else // channels >= 2
                {
                    dst_data[static_cast<size_t>(i) * channels + 0] = l;
                    dst_data[static_cast<size_t>(i) * channels + 1] = r;
                }
            }

            // Swap in resampled buffer
            std::free(sample->data);
            sample->data = dst_data;
            sample->frame_count = dst_frames;
            sample->sample_rate = k_engine_sample_rate;
        }

        return sample;
    }
} // namespace carrot::audio
