//
// Created by Zack Shrout on 2/17/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "WavStreamDecoder.h"

#include "AudioStream.h"

namespace carrot::audio {
    bool wav_stream_decoder_t::open(std::string_view path, audio_stream_t* stream) noexcept
    {
        _file = std::fopen(path.data(), "rb");
        if (!_file) return false;

        riff_header_t riff{ };
        std::fread(&riff, sizeof(riff), 1, _file);

        if (!id_equals(riff.id, "RIFF") ||
            !id_equals(riff.format, "WAVE"))
            return false;

        bool found_fmt = false;
        bool found_data = false;

        while (!std::feof(_file))
        {
            chunk_header_t chunk{ };
            if (std::fread(&chunk, sizeof(chunk), 1, _file) != 1)
                break;

            if (id_equals(chunk.id, "fmt "))
            {
                std::fread(&_fmt, sizeof(_fmt), 1, _file);
                std::fseek(_file, static_cast<int32_t>(chunk.size - sizeof(_fmt)), SEEK_CUR);
                found_fmt = true;
            }
            else if (id_equals(chunk.id, "data"))
            {
                _data_bytes_remaining = chunk.size;
                found_data = true;
                break; // file cursor now at PCM data
            }
            else
            {
                std::fseek(_file, chunk.size, SEEK_CUR);
            }
        }

        if (!found_fmt || !found_data)
            return false;

        CE_ASSERT(_fmt.audio_format == 1 || _fmt.audio_format == 3);
        CE_ASSERT(_fmt.bits_per_sample == 16 ||
            _fmt.bits_per_sample == 24 ||
            _fmt.bits_per_sample == 32);

        LOG_AUDIO_INFO("WAV file reports sample rate {} HZ in format chunk", _fmt.sample_rate);

        _stream = stream;
        _stream->channels = _fmt.num_channels;
        _stream->sample_rate = _fmt.sample_rate;

        return true;
    }

    void wav_stream_decoder_t::start() noexcept
    {
        if (_running.exchange(true, std::memory_order_acq_rel))
            return; // already running

        CE_ASSERT(_file);
        CE_ASSERT(_stream);

        _thread = std::thread([this]
        {
            this->thread_main();
        });
    }

    void wav_stream_decoder_t::stop() noexcept
    {
        if (!_running.exchange(false, std::memory_order_acq_rel))
            return; // not running

        if (_thread.joinable())
            _thread.join();

        if (_file)
        {
            std::fclose(_file);
            _file = nullptr;
        }
    }

    void wav_stream_decoder_t::thread_main() noexcept
    {
        constexpr uint32_t frames_per_chunk = 256;

        uint8_t raw[frames_per_chunk * 8]; // enough for 32-bit stereo
        float decoded[frames_per_chunk * 2];

        const uint32_t bytes_per_sample = _fmt.bits_per_sample / 8;
        const uint32_t bytes_per_frame =
                bytes_per_sample * _fmt.num_channels;

        while (_running.load(std::memory_order_acquire))
        {
            const uint32_t writable = _stream->buffer.available_write();
            if (writable == 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            const uint32_t frames =
                    std::min(writable, frames_per_chunk);

            const uint32_t bytes_to_read =
                    std::min(frames * bytes_per_frame, _data_bytes_remaining);

            if (bytes_to_read == 0)
            {
                _stream->eof.store(true, std::memory_order_release);
                break;
            }

            std::fread(raw, bytes_to_read, 1, _file);
            _data_bytes_remaining -= bytes_to_read;

            const uint32_t frames_read =
                    bytes_to_read / bytes_per_frame;

            // === CONVERSION ===

            if (_fmt.audio_format == 1 && _fmt.bits_per_sample == 16)
            {
                const int16_t* src = reinterpret_cast<int16_t *>(raw);
                for (uint32_t i = 0; i < frames_read * _fmt.num_channels; ++i)
                    decoded[i] = static_cast<float>(src[i]) / 32768.0f;
            }
            else if (_fmt.audio_format == 1 && _fmt.bits_per_sample == 24)
            {
                const uint8_t* src = raw;
                float* dst = decoded;

                for (uint32_t i = 0; i < frames_read * _fmt.num_channels; ++i)
                {
                    *dst++ = pcm24_to_float(src);
                    src += 3;
                }
            }
            else if (_fmt.audio_format == 3 && _fmt.bits_per_sample == 32)
            {
                std::memcpy(decoded, raw,
                            frames_read * _fmt.num_channels * sizeof(float));
            }

            _stream->buffer.write(decoded, frames_read);
        }
    }
} // namespace carrot::audio
