//
// Created by Zack Shrout on 2/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "WavLoader.h"

#include "WavCore.h"
#include "Audio/Core/AudioCore.h"
#include "Audio/DSP/Resampler.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace carrot::audio {
    audio_sample_t* load_wav_file(const std::string_view path)
    {
        FILE* file{ std::fopen(path.data(), "rb") };
        if (!file)
            return nullptr;

        uint8_t* pcm_data{ nullptr };
        float* samples{ nullptr };

        const auto fail = [&]() -> audio_sample_t* {
            std::free(samples);
            std::free(pcm_data);
            std::fclose(file);
            return nullptr;
        };

        riff_header_t riff{ };
        if (std::fread(&riff, sizeof(riff), 1, file) != 1)
            return fail();

        if (!id_equals(riff.id, "RIFF") || !id_equals(riff.format, "WAVE"))
            return fail();

        fmt_chunk_t fmt{ };
        bool have_fmt{ false };
        uint32_t pcm_size{ 0 };

        while (true)
        {
            chunk_header_t chunk{ };
            if (std::fread(&chunk, sizeof(chunk), 1, file) != 1)
                break;

            if (id_equals(chunk.id, "fmt "))
            {
                if (chunk.size < sizeof(fmt))
                    return fail();

                if (std::fread(&fmt, sizeof(fmt), 1, file) != 1)
                    return fail();

                const uint32_t remaining{ chunk.size - static_cast<uint32_t>(sizeof(fmt)) };
                if (remaining > 0)
                {
                    if (std::fseek(file, static_cast<long>(remaining), SEEK_CUR) != 0)
                        return fail();
                }

                have_fmt = true;
            }
            else if (id_equals(chunk.id, "data"))
            {
                if (pcm_data != nullptr)
                    return fail(); // duplicate data chunk not supported

                if (chunk.size == 0)
                    return fail();

                pcm_size = chunk.size;
                pcm_data = static_cast<uint8_t *>(std::malloc(pcm_size));
                if (!pcm_data)
                    return fail();

                if (std::fread(pcm_data, pcm_size, 1, file) != 1)
                    return fail();
            }
            else
            {
                if (std::fseek(file, static_cast<long>(chunk.size), SEEK_CUR) != 0)
                    return fail();
            }

            // RIFF chunks are word-aligned; odd-sized chunks include a pad byte.
            if ((chunk.size & 1u) != 0u)
            {
                if (std::fseek(file, 1, SEEK_CUR) != 0)
                    return fail();
            }
        }

        std::fclose(file);
        file = nullptr; // prevent accidental reuse in fail pattern below

        if (!have_fmt || !pcm_data)
        {
            std::free(pcm_data);
            return nullptr;
        }

        if (fmt.sample_rate == 0)
        {
            std::free(pcm_data);
            return nullptr;
        }

        if (fmt.num_channels == 0 || fmt.num_channels > 2)
        {
            std::free(pcm_data);
            return nullptr;
        }

        if (fmt.audio_format != 1 && fmt.audio_format != 3)
        {
            std::free(pcm_data);
            return nullptr;
        }

        const bool supported_format{
            (fmt.audio_format == 1 && fmt.bits_per_sample == 16) || (fmt.audio_format == 1 && fmt.bits_per_sample == 24)
            || (fmt.audio_format == 3 && fmt.bits_per_sample == 32)
        };

        if (!supported_format)
        {
            std::free(pcm_data);
            return nullptr;
        }

        const uint32_t bytes_per_sample{ static_cast<uint32_t>(fmt.bits_per_sample / 8) };
        if (bytes_per_sample == 0)
        {
            std::free(pcm_data);
            return nullptr;
        }

        const uint32_t bytes_per_frame{ fmt.num_channels * bytes_per_sample };
        if (bytes_per_frame == 0 || (pcm_size % bytes_per_frame) != 0)
        {
            std::free(pcm_data);
            return nullptr;
        }

        const uint32_t frame_count{ pcm_size / bytes_per_frame };
        const size_t total_samples{ static_cast<size_t>(frame_count) * static_cast<size_t>(fmt.num_channels) };

        samples = static_cast<float *>(std::malloc(sizeof(float) * total_samples));
        if (!samples)
        {
            std::free(pcm_data);
            return nullptr;
        }

        if (fmt.audio_format == 1 && fmt.bits_per_sample == 16)
        {
            const int16_t* src{ reinterpret_cast<const int16_t *>(pcm_data) };
            for (size_t i{ 0 }; i < total_samples; ++i)
                samples[i] = static_cast<float>(src[i]) / 32768.f;
        }
        else if (fmt.audio_format == 1 && fmt.bits_per_sample == 24)
        {
            const uint8_t* src{ pcm_data };
            float* dst{ samples };

            for (size_t i{ 0 }; i < total_samples; ++i)
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
        pcm_data = nullptr;

        audio_sample_t* sample{ new audio_sample_t{ } };
        sample->data = samples;
        sample->frame_count = frame_count;
        sample->channels = fmt.num_channels;
        sample->sample_rate = fmt.sample_rate;

        samples = nullptr; // ownership transferred to sample

        // Offline resample to engine mix rate (48k) for sample-based assets
        if (sample->sample_rate != k_engine_sample_rate &&
            sample->frame_count > 0 &&
            sample->channels > 0 &&
            sample->data)
        {
            const double src_rate{ static_cast<double>(sample->sample_rate) };
            constexpr double dst_rate{ static_cast<double>(k_engine_sample_rate) };

            const uint32_t src_frames{ sample->frame_count };
            const uint32_t channels{ sample->channels };

            const double frame_ratio{ dst_rate / src_rate };
            const uint32_t dst_frames{
                static_cast<uint32_t>(std::ceil(static_cast<double>(src_frames) * frame_ratio))
            };

            float* dst_data{
                static_cast<float *>(std::malloc(sizeof(float) * static_cast<size_t>(dst_frames) * channels))
            };

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
            req.src_step = src_rate / dst_rate;
            req.looping = false;
            req.loop.start = 0;
            req.loop.end = src_frames;

            for (uint32_t i{ 0 }; i < dst_frames; ++i)
            {
                float l{ 0.f };
                float r{ 0.f };

                if (!resample_linear_frame(req, l, r))
                {
                    for (uint32_t ch{ 0 }; ch < channels; ++ch)
                        dst_data[static_cast<size_t>(i) * channels + ch] = 0.f;

                    continue;
                }

                if (channels == 1)
                {
                    dst_data[static_cast<size_t>(i) * channels + 0] = l;
                }
                else // channels == 2
                {
                    dst_data[static_cast<size_t>(i) * channels + 0] = l;
                    dst_data[static_cast<size_t>(i) * channels + 1] = r;
                }
            }

            std::free(sample->data);
            sample->data = dst_data;
            sample->frame_count = dst_frames;
            sample->sample_rate = k_engine_sample_rate;
        }

        return sample;
    }
} // namespace carrot::audio
