//
// Created by Zack Shrout on 2/17/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "WavStreamDecoder.h"

#include "AudioStream.h"
#include "Utils/File/FileUtils.h"

namespace carrot::audio {
    namespace {
        [[nodiscard]] bool parse_wav_stream_header(FILE* file,
                                                   fmt_chunk_t& out_fmt,
                                                   uint64_t& out_data_bytes_total,
                                                   carrot_offset_t& out_data_start_offset) noexcept
        {
            if (!file)
                return false;

            riff_header_t riff{ };
            if (std::fread(&riff, sizeof(riff), 1, file) != 1)
                return false;

            if (!id_equals(riff.id, "RIFF") || !id_equals(riff.format, "WAVE"))
                return false;

            bool found_fmt{ false };
            bool found_data{ false };

            while (true)
            {
                chunk_header_t chunk{ };
                if (std::fread(&chunk, sizeof(chunk), 1, file) != 1)
                    break;

                if (id_equals(chunk.id, "fmt "))
                {
                    if (chunk.size < sizeof(out_fmt))
                        return false;

                    if (std::fread(&out_fmt, sizeof(out_fmt), 1, file) != 1)
                        return false;

                    const uint32_t remaining{ chunk.size - static_cast<uint32_t>(sizeof(out_fmt)) };
                    if (remaining > 0)
                    {
                        if (utils::file::seek_file(file, static_cast<utils::file::file_offset_t>(remaining), SEEK_CUR) != 0)
                            return false;
                    }

                    found_fmt = true;
                }
                else if (id_equals(chunk.id, "data"))
                {
                    if (chunk.size == 0)
                        return false;

                    out_data_bytes_total = chunk.size;
                    out_data_start_offset = static_cast<carrot_offset_t>(utils::file::tell_file(file));
                    if (out_data_start_offset < 0)
                        return false;

                    found_data = true;
                    break;
                }
                else
                {
                    if (utils::file::seek_file(file, static_cast<utils::file::file_offset_t>(chunk.size), SEEK_CUR) != 0)
                        return false;
                }

                if ((chunk.size & 1u) != 0u)
                {
                    if (utils::file::seek_file(file, static_cast<utils::file::file_offset_t>(1), SEEK_CUR) != 0)
                        return false;
                }
            }

            if (!found_fmt || !found_data)
                return false;

            if (out_fmt.audio_format != 1 && out_fmt.audio_format != 3)
                return false;

            const bool supported_format =
                (out_fmt.audio_format == 1 && out_fmt.bits_per_sample == 16) ||
                (out_fmt.audio_format == 1 && out_fmt.bits_per_sample == 24) ||
                (out_fmt.audio_format == 3 && out_fmt.bits_per_sample == 32);
            if (!supported_format)
                return false;

            if (out_fmt.sample_rate == 0)
                return false;

            if (out_fmt.num_channels == 0 || out_fmt.num_channels > 2 || out_fmt.num_channels > k_max_channels)
                return false;

            const uint32_t bytes_per_sample{ static_cast<uint32_t>(out_fmt.bits_per_sample / 8u) };
            if (bytes_per_sample == 0)
                return false;

            const uint32_t bytes_per_frame{ bytes_per_sample * out_fmt.num_channels };
            if (bytes_per_frame == 0 || (out_data_bytes_total % bytes_per_frame) != 0)
                return false;

            return true;
        }
    }

    // PUBLIC
    bool wav_stream_decoder_t::open(const std::string_view path, audio_stream_t* stream) noexcept
    {
        stop(); // ensure prior thread/file are fully released before reuse

        _stream = nullptr;
        _fmt = { };
        _data_bytes_remaining = 0;
        _data_bytes_total = 0;
        _data_start_offset = 0;
        _loop_start_offset = 0;
        _loop_end_offset = 0;
        _use_loop_region = false;
        _in_loop_phase = false;
        _src_sample_rate = 0;
        _src_frames_total = 0;
        _src_frames_in_buffer = 0;
        _src_pos = 0.0;

        const auto fail = [&]() -> bool {
            if (_file)
            {
                std::fclose(_file);
                _file = nullptr;
            }

            _stream = nullptr;
            _fmt = { };
            _data_bytes_remaining = 0;
            _data_bytes_total = 0;
            _data_start_offset = 0;
            _loop_start_offset = 0;
            _loop_end_offset = 0;
            _use_loop_region = false;
            _in_loop_phase = false;
            _src_sample_rate = 0;
            _src_frames_total = 0;
            _src_frames_in_buffer = 0;
            _src_pos = 0.0;

            return false;
        };

        const auto prepared{ prepare(path) };
        if (!prepared)
            return fail();

        return open_prepared(*prepared, stream);
    }

    bool wav_stream_decoder_t::open_prepared(const prepared_wav_stream_t& prepared, audio_stream_t* stream) noexcept
    {
        stop();

        _file = utils::file::open_file(prepared.path.string().c_str(), "rb");
        if (!_file)
            return false;

        _stream = stream;
        if (!_stream)
            return false;

        _fmt = prepared.fmt;
        _data_bytes_remaining = prepared.data_bytes_total;
        _data_bytes_total = prepared.data_bytes_total;
        _data_start_offset = prepared.data_start_offset;
        _src_sample_rate = prepared.source_sample_rate;
        _src_frames_total = prepared.source_frames_total;
        _src_pos = 0.0;
        _src_frames_in_buffer = 0;

        if (utils::file::seek_file(_file, prepared.data_start_offset, SEEK_SET) != 0)
            return false;

        LOG_AUDIO_INFO("WAV file reports sample rate {} HZ in format chunk", _fmt.sample_rate);

        _stream->channels = _fmt.num_channels;
        _stream->sample_rate = k_engine_sample_rate;
        return true;
    }

    std::optional<prepared_wav_stream_t> wav_stream_decoder_t::prepare(const std::string_view path) noexcept
    {
        FILE* file{ utils::file::open_file(path.data(), "rb") };
        if (!file)
            return std::nullopt;

        prepared_wav_stream_t prepared;
        prepared.path = std::filesystem::path{ path };
        if (!parse_wav_stream_header(file, prepared.fmt, prepared.data_bytes_total, prepared.data_start_offset))
        {
            std::fclose(file);
            return std::nullopt;
        }

        const uint32_t bytes_per_sample{ static_cast<uint32_t>(prepared.fmt.bits_per_sample / 8u) };
        const uint32_t bytes_per_frame{ bytes_per_sample * prepared.fmt.num_channels };
        prepared.source_sample_rate = prepared.fmt.sample_rate;
        prepared.source_frames_total = prepared.data_bytes_total / bytes_per_frame;

        std::fclose(file);
        return prepared;
    }

    uint32_t wav_stream_decoder_t::prime_initial_buffer(const uint32_t target_frames) noexcept
    {
        if (!_stream || !_file || target_frames == 0u)
            return 0u;

        const uint32_t bytes_per_sample{ static_cast<uint32_t>(_fmt.bits_per_sample / 8u) };
        const uint32_t bytes_per_frame{ bytes_per_sample * _fmt.num_channels };
        if (bytes_per_frame == 0u)
            return 0u;

        init_loop_region(bytes_per_frame);

        uint32_t primed_frames{ 0u };
        while (primed_frames < target_frames)
        {
            const uint32_t writable{
                chlm::min<uint32_t>(_stream->buffer.available_write(), target_frames - primed_frames)
            };
            if (writable == 0u)
                break;

            fill_src_buffer(bytes_per_frame);
            if (_src_frames_in_buffer == 0u)
                break;

            const uint32_t produced{ produce_resampled_chunk(writable) };
            if (produced == 0u)
                break;

            primed_frames += produced;
        }

        return primed_frames;
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
        if (_running.exchange(false, std::memory_order_acq_rel) && _thread.joinable())
            _thread.join();

        if (_file)
        {
            std::fclose(_file);
            _file = nullptr;
        }
    }

    // PRIVATE
    void wav_stream_decoder_t::thread_main() noexcept
    {
        const uint32_t bytes_per_sample{ static_cast<uint32_t>(_fmt.bits_per_sample / 8) };
        const uint32_t bytes_per_frame{ bytes_per_sample * _fmt.num_channels };

        bool loop_initialized{ false };

        while (_running.load(std::memory_order_acquire))
        {
            if (!_stream) break;

            // --- One-time loop region setup ---
            if (!loop_initialized)
            {
                init_loop_region(bytes_per_frame);
                loop_initialized = true;
            }

            const uint32_t writable{ _stream->buffer.available_write() };
            if (writable == 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            // 1) Make sure we have some source PCM ready
            fill_src_buffer(bytes_per_frame);

            if (_src_frames_in_buffer == 0)
            {
                // No source frames; if EOF is set, we're truly done.
                if (_stream->eof.load(std::memory_order_acquire))
                    break;

                // Otherwise, loop back around to decode more (e.g. after a loop wrap).
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            // 2) Resample from source buffer at file rate into 48k ring buffer
            const uint32_t produced_48k{ produce_resampled_chunk(writable) };

            if (produced_48k == 0)
            {
                // Could happen if resampler ran out of source data mid-chunk;
                // let the loop try decoding more.
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

    void wav_stream_decoder_t::init_loop_region(const uint32_t bytes_per_frame) noexcept
    {
        if (!_stream->looping || (!_stream->loop_start && !_stream->loop_end))
        {
            _use_loop_region = false;
            _in_loop_phase = false;
            return;
        }

        _use_loop_region = true;

        const uint64_t total_frames{ _data_bytes_total / bytes_per_frame };
        const uint64_t loop_start_frame{ chlm::min<uint64_t>(_stream->loop_start, total_frames) };

        uint64_t loop_end_frame{
            chlm::min<uint64_t>(_stream->loop_end ? _stream->loop_end : total_frames, total_frames)
        };

        if (loop_end_frame < loop_start_frame)
            loop_end_frame = loop_start_frame;

        _loop_start_offset = _data_start_offset + static_cast<carrot_offset_t>(
                                 loop_start_frame * bytes_per_frame);

        _loop_end_offset = _data_start_offset + static_cast<carrot_offset_t>(
                               loop_end_frame * bytes_per_frame);

        // Start outside the loop phase; initial pass may include pre-roll
        _in_loop_phase = false;
    }

    void wav_stream_decoder_t::enter_loop_phase() noexcept
    {
        if (utils::file::seek_file(_file, static_cast<utils::file::file_offset_t>(_loop_start_offset), SEEK_SET) != 0)
        {
            if (_stream)
                _stream->eof.store(true, std::memory_order_release);
            return;
        }

        const uint64_t bytes_from_loop_start{ static_cast<uint64_t>(_loop_end_offset - _loop_start_offset) };

        _data_start_offset = _loop_start_offset;
        _data_bytes_total = bytes_from_loop_start;
        _data_bytes_remaining = _data_bytes_total;
        _in_loop_phase = true;
    }

    void wav_stream_decoder_t::fill_src_buffer(const uint32_t bytes_per_frame) noexcept
    {
        if (!_stream || !_file || bytes_per_frame == 0)
            return;

        while (_src_frames_in_buffer < k_src_buffer_frames &&
               _running.load(std::memory_order_acquire))
        {
            if (_data_bytes_remaining == 0)
            {
                if (_stream->looping)
                {
                    if (_use_loop_region)
                    {
                        enter_loop_phase();
                    }
                    else
                    {
                        if (utils::file::seek_file(_file, static_cast<utils::file::file_offset_t>(_data_start_offset), SEEK_SET) != 0)
                        {
                            _stream->eof.store(true, std::memory_order_release);
                            break;
                        }

                        _data_bytes_remaining = _data_bytes_total;
                        _in_loop_phase = true;
                    }

                    continue;
                }
                else
                {
                    _stream->eof.store(true, std::memory_order_release);
                    break;
                }
            }

            const uint32_t frames_to_decode = chlm::min<uint32_t>(
                k_frames_per_decode_chunk,
                k_src_buffer_frames - _src_frames_in_buffer
            );

            if (frames_to_decode == 0)
                break;

            const uint64_t bytes_request{ static_cast<uint64_t>(frames_to_decode) * bytes_per_frame };
            uint64_t bytes_to_read{ chlm::min<uint64_t>(bytes_request, _data_bytes_remaining) };

            const carrot_offset_t current_offset{
                _data_start_offset +
                static_cast<carrot_offset_t>(_data_bytes_total - _data_bytes_remaining)
            };

            if (_use_loop_region)
            {
                const carrot_offset_t logical_loop_end{ _loop_end_offset };
                const uint64_t bytes_until_loop_end{
                    static_cast<uint64_t>(logical_loop_end - current_offset)
                };

                if (bytes_until_loop_end < bytes_to_read)
                    bytes_to_read = bytes_until_loop_end;
            }

            if (bytes_to_read == 0)
            {
                if (_stream->looping)
                {
                    if (_use_loop_region)
                    {
                        enter_loop_phase();
                    }
                    else
                    {
                        if (utils::file::seek_file(_file, static_cast<utils::file::file_offset_t>(_data_start_offset), SEEK_SET) != 0)
                        {
                            _stream->eof.store(true, std::memory_order_release);
                            break;
                        }

                        _data_bytes_remaining = _data_bytes_total;
                        _in_loop_phase = true;
                    }

                    continue;
                }
                else
                {
                    _stream->eof.store(true, std::memory_order_release);
                    break;
                }
            }

            if ((bytes_to_read % bytes_per_frame) != 0)
            {
                _stream->eof.store(true, std::memory_order_release);
                break;
            }

            uint8_t raw[k_frames_per_decode_chunk * 8]{ };

            if (std::fread(raw, bytes_to_read, 1, _file) != 1)
            {
                _stream->eof.store(true, std::memory_order_release);
                break;
            }

            _data_bytes_remaining -= bytes_to_read;

            const uint32_t frames_read{
                static_cast<uint32_t>(bytes_to_read / bytes_per_frame)
            };

            if (frames_read == 0)
                continue;

            float* dst{ &_src_buffer[_src_frames_in_buffer * _fmt.num_channels] };

            if (_fmt.audio_format == 1 && _fmt.bits_per_sample == 16)
            {
                const int16_t* src{ reinterpret_cast<const int16_t *>(raw) };
                for (uint32_t i{ 0 }; i < frames_read * _fmt.num_channels; ++i)
                    dst[i] = static_cast<float>(src[i]) / 32768.0f;
            }
            else if (_fmt.audio_format == 1 && _fmt.bits_per_sample == 24)
            {
                const uint8_t* src{ raw };
                for (uint32_t i{ 0 }; i < frames_read * _fmt.num_channels; ++i)
                {
                    dst[i] = pcm24_to_float(src);
                    src += 3;
                }
            }
            else if (_fmt.audio_format == 3 && _fmt.bits_per_sample == 32)
            {
                std::memcpy(dst, raw, static_cast<size_t>(frames_read) * _fmt.num_channels * sizeof(float));
            }
            else
            {
                _stream->eof.store(true, std::memory_order_release);
                break;
            }

            _src_frames_in_buffer += frames_read;
        }

        if (!_stream->looping && _data_bytes_remaining == 0 && _src_frames_in_buffer == 0)
        {
            _stream->eof.store(true, std::memory_order_release);
        }
    }

    uint32_t wav_stream_decoder_t::produce_resampled_chunk(const uint32_t writable_48k) noexcept
    {
        if (!_stream || _src_frames_in_buffer == 0 || writable_48k == 0)
            return 0;

        const uint32_t channels{ _fmt.num_channels };
        const uint32_t max_frames_to_write{ chlm::min<uint32_t>(writable_48k, k_max_48k_chunk) };

        if (max_frames_to_write == 0)
            return 0;

        float out48k[k_max_48k_chunk * k_max_channels];

        resample_request_t req{ };
        req.data = _src_buffer;
        req.total_frames = _src_frames_in_buffer;
        req.channels = channels;
        req.src_pos = _src_pos;
        req.src_step = static_cast<double>(_src_sample_rate) / static_cast<double>(k_engine_sample_rate);
        req.looping = false; // loop handled via file position, not resampler
        req.loop.start = 0;
        req.loop.end = _src_frames_in_buffer;

        uint32_t produced{ 0 };

        for (; produced < max_frames_to_write; ++produced)
        {
            float l{ 0.f };
            float r{ 0.f };

            if (!resample_linear_frame(req, l, r))
            {
                // Ran out of source samples in the staging buffer;
                // outer loop will refill _src_buffer on the next iteration.
                break;
            }

            if (channels == 1)
            {
                out48k[produced * channels + 0] = l;
            }
            else // channels >= 2
            {
                out48k[produced * channels + 0] = l;
                out48k[produced * channels + 1] = r;
            }
        }

        // Commit updated source position
        _src_pos = req.src_pos;

        if (produced > 0)
            _stream->buffer.write(out48k, produced);

        // Drop consumed frames from the source buffer
        slide_consumed_source_frames();

        return produced;
    }

    void wav_stream_decoder_t::slide_consumed_source_frames() noexcept
    {
        const uint32_t channels{ _fmt.num_channels };

        // src_pos is in source frames; any whole frames before floor(src_pos)
        // are no longer needed for forward resampling.
        const uint32_t consumed{ static_cast<uint32_t>(_src_pos) };

        if (consumed == 0 || consumed > _src_frames_in_buffer)
            return;

        const uint32_t remaining{ _src_frames_in_buffer - consumed };

        if (remaining > 0)
        {
            std::memmove(
                _src_buffer,
                &_src_buffer[consumed * channels],
                remaining * channels * sizeof(float)
            );
        }

        _src_frames_in_buffer = remaining;
        _src_pos -= static_cast<double>(consumed);
    }
} // namespace carrot::audio
