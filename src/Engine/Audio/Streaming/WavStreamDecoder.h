//
// Created by Zack Shrout on 2/17/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Audio/Core/AudioCore.h"
#include "Audio/DSP/Resampler.h"
#include "Audio/Sample/WavCore.h"

#include <atomic>
#include <thread>

namespace carrot::audio {
#if defined(_WIN32)
    using carrot_offset_t = __int64;
#else
    using carrot_offset_t = off_t; ///< Typically 64-bit with _FILE_OFFSET_BITS=64
#endif

    struct audio_stream_t;

    /**
     * @brief Incremental WAV streaming decoder for audio_stream_t.
     *
     * This class owns a background thread that:
     *  - Reads and decodes PCM data from a WAV file
     *  - Optionally enforces a loop region (start/end in frames)
     *  - Pushes decoded samples into an audio_stream_t ring buffer
     *
     * Threading model:
     *  - @ref open() and @ref start() are called from the engine / non-RT thread.
     *  - @ref thread_main() runs on a dedicated decode thread.
     *  - The audio thread only touches audio_stream_t::buffer and eof/looping flags.
     *
     * Lifetime:
     *  - open() must succeed before start().
     *  - start() spawns the decode thread once; multiple calls are ignored.
     *  - stop() joins the decode thread (if running) and closes the file.
     *  - The destructor automatically calls stop().
     */
    class wav_stream_decoder_t
    {
    public:
        /**
         * @brief Destructor.
         *
         * Ensures the decode thread is stopped and the file handle is closed
         * by calling @ref stop().
         */
        ~wav_stream_decoder_t() { stop(); }

        /**
         * @brief Opens a WAV file and binds it to an audio_stream_t.
         *
         * This parses the RIFF/WAVE headers, finds the "fmt " and "data" chunks,
         * validates the format, and initializes internal counters for streaming.
         *
         * On success:
         *  - The decoder holds a FILE* to @p path.
         *  - @p stream->channels and @p stream->sample_rate are populated from the file.
         *  - The file position is left at the start of PCM data.
         *
         * @param path   Filesystem path to the WAV file.
         * @param stream Target audio_stream_t that will receive decoded data.
         * @return true if the file was opened and parsed successfully; false otherwise.
         *
         * @note This function does not start the decode thread. Call @ref start()
         *       after a successful open().
         */
        bool open(std::string_view path, audio_stream_t* stream) noexcept;

        /**
         * @brief Starts the background decode thread.
         *
         * If the thread is already running, this is a no-op.
         *
         * Preconditions:
         *  - @ref open() has succeeded.
         *  - The underlying FILE* and audio_stream_t pointer are valid.
         *
         * Thread responsibilities:
         *  - Continuously decode chunks of PCM data.
         *  - Respect the stream's looping flags and loop region (if any).
         *  - Write decoded frames into the stream's ring buffer.
         */
        void start() noexcept;

        /**
         * @brief Stops the decode thread and closes the underlying file.
         *
         * Behavior:
         *  - If the thread is running, signals it to exit and joins it.
         *  - If the file is open, closes it and clears the FILE*.
         *
         * It is safe to call stop() even if open() failed or start() was
         * never called; in that case it simply ensures no resources are held.
         */
        void stop() noexcept;

    private:
        /**
         * @brief Main decode loop run on the background thread.
         *
         * Responsibilities:
         *  - Reads PCM blocks from the WAV file.
         *  - Converts them to float samples (16/24/32-bit PCM).
         *  - Writes decoded frames into audio_stream_t::buffer.
         *  - Sets audio_stream_t::eof when a non-looping stream finishes.
         *
         * Loop behavior:
         *  - If the stream is marked looping and a loop region is configured,
         *    the decoder will repeatedly wrap between @ref _loop_start_offset
         *    and @ref _loop_end_offset.
         *  - If looping is enabled but no loop region is set, the entire data
         *    chunk is looped.
         */
        void thread_main() noexcept;

        /**
         * @brief Initializes the loop region based on the bound audio_stream_t.
         *
         * Uses the stream's loop_start / loop_end (in frames) in conjunction
         * with the WAV data size to compute:
         *  - @ref _loop_start_offset : byte offset of the loop start
         *  - @ref _loop_end_offset   : byte offset one past the loop end
         *
         * Semantics:
         *  - If the stream is non-looping or has a trivial loop region
         *    (start == 0 && end == 0), looping is disabled and the decoder
         *    plays the full data chunk once.
         *  - Loop start/end are clamped to the valid frame range.
         *
         * @param bytes_per_frame Number of bytes in a single PCM frame
         *                        (channels * bytes per sample).
         */
        void init_loop_region(uint32_t bytes_per_frame) noexcept;

        /**
         * @brief Enters the loop phase and re-bases the logical data segment.
         *
         * After this call:
         *  - The file cursor is positioned at @ref _loop_start_offset.
         *  - @ref _data_start_offset refers to the loop start.
         *  - @ref _data_bytes_total and @ref _data_bytes_remaining describe
         *    only the loop region [loop_start, loop_end).
         *  - @ref _in_loop_phase is set to true.
         *
         * This is called whenever the decoder either:
         *  - Reaches the physical end of the file while looping is enabled, or
         *  - Hits the logical loop_end boundary while reading.
         */
        void enter_loop_phase() noexcept;

        /**
         * @brief Top off the source PCM staging buffer from the WAV file.
         *
         * - Decodes at most k_src_buffer_frames - _src_frames_in_buffer frames.
         * - Honors loop regions and looping vs non-looping behavior.
         * - Sets _stream->eof when a non-looping stream finishes.
         *
         * @param bytes_per_frame Number of bytes per PCM frame in the file.
         */
        void fill_src_buffer(uint32_t bytes_per_frame) noexcept;

        /**
         * @brief Resample from _src_buffer into the stream ring buffer at 48k.
         *
         * - Uses resample_linear_frame() to produce up to @p writable_48k frames.
         * - Writes produced frames to _stream->buffer.
         * - Slides consumed source frames out of _src_buffer.
         *
         * @param writable_48k Maximum number of frames available in the ring buffer.
         * @return Number of 48k frames produced and written.
         */
        uint32_t produce_resampled_chunk(uint32_t writable_48k) noexcept;

        /**
         * @brief Slide fully consumed source frames out of _src_buffer.
         *
         * Uses _src_pos to determine how many whole frames are no longer needed,
         * memmoves remaining frames to the front, and keeps the fractional portion
         * of _src_pos.
         */
        void slide_consumed_source_frames() noexcept;

        /** Background decode thread. */
        std::thread _thread;

        /** Flag indicating whether the decode thread should be running. */
        std::atomic<bool> _running{ false };

        /** Underlying file handle for the opened WAV. */
        FILE* _file{ nullptr };

        /** Target audio stream that receives decoded samples. */
        audio_stream_t* _stream{ nullptr };

        /** Parsed "fmt " chunk describing WAV format (channels, bits, etc.). */
        fmt_chunk_t _fmt{ };

        /**
         * Remaining bytes in the current logical data segment.
         * Initially the size of the entire data chunk; later may be rebased
         * to the loop region only.
         */
        uint64_t _data_bytes_remaining{ 0 };

        /**
         * Total bytes in the current logical data segment.
         * Initially the size of the entire data chunk; later may be set
         * to the size of the loop region when in loop phase.
         */
        uint64_t _data_bytes_total{ 0 };

        /**
         * Byte offset of the current logical data start within the file.
         * Initially the start of the "data" chunk; later may be rebased
         * to @ref _loop_start_offset when entering loop phase.
         */
        carrot_offset_t _data_start_offset{ 0 };

        /** Byte offset of the loop region start within the file. */
        carrot_offset_t _loop_start_offset{ 0 };

        /** Byte offset one past the end of the loop region within the file. */
        carrot_offset_t _loop_end_offset{ 0 };

        /** True if a non-trivial loop region is configured for this stream. */
        bool _use_loop_region{ false };

        /**
         * True once the decoder has entered the loop phase (i.e., it is
         * now reading exclusively from the loop region segment).
         */
        bool _in_loop_phase{ false };

        /** Source (file) sample rate cached from fmt chunk. */
        uint32_t _src_sample_rate{ 0 };

        /**
         * Resampler state for streaming:
         *  - We decode PCM into _src_buffer at source sample rate.
         *  - We consume from _src_buffer via linear interpolation and
         *    write 48k frames into the stream ring buffer.
         */
        static constexpr uint32_t k_src_buffer_frames{ 1024 };

        float _src_buffer[k_src_buffer_frames * k_max_channels]{ };
        uint32_t _src_frames_in_buffer{ 0 }; ///< Valid frames currently in _src_buffer

        /** Global position in source frames for resampling (fractional). */
        double _src_pos{ 0.0 };

        /** Total source frames in the file (for optional EOF reasoning). */
        uint64_t _src_frames_total{ 0 };

        static constexpr uint32_t k_frames_per_decode_chunk{ 256 };
        static constexpr uint32_t k_max_48k_chunk{ 256 };
    };
} // namespace carrot::audio
