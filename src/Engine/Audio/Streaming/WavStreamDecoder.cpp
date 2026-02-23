//
// Created by Zack Shrout on 2/17/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "WavStreamDecoder.h"

#include "AudioStream.h"

#if defined(_WIN32)
#define carrot_fseek  _fseeki64
#define carrot_ftell  _ftelli64
#else
#define carrot_fseek  fseeko
#define carrot_ftell  ftello
#endif

namespace carrot::audio {
    bool wav_stream_decoder_t::open(const std::string_view path, audio_stream_t* stream) noexcept
    {
        _file = std::fopen(path.data(), "rb");
        if (!_file) return false;

        riff_header_t riff{ };
        std::fread(&riff, sizeof(riff), 1, _file);

        if (!id_equals(riff.id, "RIFF") || !id_equals(riff.format, "WAVE"))
            return false;

        bool found_fmt{ false };
        bool found_data{ false };

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
                _data_bytes_total = chunk.size;
                _data_start_offset = carrot_ftell(_file);

                found_data = true;
                break; // file cursor now at PCM data
            }
            else
            {
                carrot_fseek(_file, chunk.size, SEEK_CUR);
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

        _thread = std::thread([this] {
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
        constexpr uint32_t frames_per_chunk{ 256 };

        uint8_t raw[frames_per_chunk * 8]; // enough for 32-bit stereo
        float decoded[frames_per_chunk * 2];

        const uint32_t bytes_per_sample{ static_cast<uint32_t>(_fmt.bits_per_sample / 8) };
        const uint32_t bytes_per_frame{ bytes_per_sample * _fmt.num_channels };

        bool loop_initialized{ false };

        while (_running.load(std::memory_order_acquire))
        {
            if (!_stream) break;

            // --- One-time loop region setup ---
            if (!loop_initialized)
            {
                loop_initialized = true;

                if (_stream->looping && (_stream->loop_start > 0 || _stream->loop_end > 0))
                {
                    _use_loop_region = true;

                    LOG_AUDIO_INFO("WAV file reports loop region {} - {}", _stream->loop_start, _stream->loop_end);

                    const uint64_t total_frames{ _data_bytes_total / bytes_per_frame };
                    const uint64_t loop_start_frame{ std::min<uint64_t>(_stream->loop_start, total_frames) };
                    uint64_t loop_end_frame{
                        std::min<uint64_t>(_stream->loop_end ? _stream->loop_end : total_frames, total_frames)
                    };

                    if (loop_end_frame < loop_start_frame)
                        loop_end_frame = loop_start_frame;

                    _loop_start_offset = _data_start_offset + static_cast<carrot_offset_t>(
                                             loop_start_frame * bytes_per_frame);

                    _loop_end_offset = _data_start_offset + static_cast<carrot_offset_t>(
                                           loop_end_frame * bytes_per_frame);

                    // We are *not yet* in the loop phase: we start from _data_start_offset
                    _in_loop_phase = false;

                    LOG_AUDIO_INFO("Loop region start offset: {}, end offset: {}", _loop_start_offset,
                                   _loop_end_offset);
                    LOG_AUDIO_INFO("Total data bytes: {}", _data_bytes_total);
                }
                else
                {
                    _use_loop_region = false;
                    _in_loop_phase = false;
                }
            }

            const uint32_t writable{ _stream->buffer.available_write() };
            if (writable == 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            const uint32_t frames{ std::min(writable, frames_per_chunk) };
            const uint64_t bytes_to_read{ std::min<uint64_t>(frames * bytes_per_frame, _data_bytes_remaining) };

            if (bytes_to_read == 0)
            {
                if (_stream->looping)
                {
                    if (_use_loop_region)
                    {
                        // If we had a loop region but hit EOF before _loop_end_offset,
                        // just wrap into the loop region proper.
                        enter_loop_phase();
                    }
                    else
                    {
                        // Full-file loop case (no special region)
                        carrot_fseek(_file, _data_start_offset, SEEK_SET);
                        _data_bytes_remaining = _data_bytes_total;
                        _in_loop_phase = true;
                    }

                    continue;
                }

                _stream->eof.store(true, std::memory_order_release);
                break;
            }

            // Compute current file position *before* this read:
            const carrot_offset_t current_offset{
                _data_start_offset + static_cast<carrot_offset_t>(_data_bytes_total - _data_bytes_remaining)
            };

            // Clamp read size to not overshoot loop_end_offset when in loop mode
            uint64_t clamped_bytes_to_read = bytes_to_read;

            if (_use_loop_region)
            {
                const carrot_offset_t logical_loop_end{ _loop_end_offset };

                const auto bytes_until_loop_end{ static_cast<uint64_t>(logical_loop_end - current_offset) };

                if (bytes_until_loop_end < clamped_bytes_to_read)
                    clamped_bytes_to_read = bytes_until_loop_end;
            }

            if (clamped_bytes_to_read == 0)
            {
                // Hit loop boundary exactly: wrap into loop region
                enter_loop_phase();
                continue;
            }

            std::fread(raw, clamped_bytes_to_read, 1, _file);
            _data_bytes_remaining -= clamped_bytes_to_read;

            const uint32_t frames_read{ static_cast<uint32_t>(clamped_bytes_to_read / bytes_per_frame) };

            // === CONVERSION ===

            if (_fmt.audio_format == 1 && _fmt.bits_per_sample == 16)
            {
                const int16_t* src{ reinterpret_cast<int16_t *>(raw) };

                for (uint32_t i = 0; i < frames_read * _fmt.num_channels; ++i)
                    decoded[i] = static_cast<float>(src[i]) / 32768.0f;
            }
            else if (_fmt.audio_format == 1 && _fmt.bits_per_sample == 24)
            {
                const uint8_t* src{ raw };
                float* dst{ decoded };

                for (uint32_t i = 0; i < frames_read * _fmt.num_channels; ++i)
                {
                    *dst++ = pcm24_to_float(src);
                    src += 3;
                }
            }
            else if (_fmt.audio_format == 3 && _fmt.bits_per_sample == 32)
            {
                std::memcpy(decoded, raw, frames_read * _fmt.num_channels * sizeof(float));
            }

            _stream->buffer.write(decoded, frames_read);
        }
    }

    void wav_stream_decoder_t::enter_loop_phase() noexcept
    {
        // NOTE: Function assumes we already have _loop_start_offset and _loop_end_offset computed.

        carrot_fseek(_file, _loop_start_offset, SEEK_SET);

        const uint64_t bytes_from_loop_start{ static_cast<uint64_t>(_loop_end_offset - _loop_start_offset) };

        // Rebase logical data segment to the loop region
        _data_start_offset = _loop_start_offset;
        _data_bytes_total = bytes_from_loop_start;
        _data_bytes_remaining = _data_bytes_total;

        _in_loop_phase = true;
    }
} // namespace carrot::audio
