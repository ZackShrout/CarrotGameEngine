//
// Created by Zack Shrout on 2/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Core/Logger.h"

#include <chlm/CarrotHLM.h>
#include <cmath>
#include <cstdint>

namespace carrot::audio {
    /**
     * @brief Loop region for resampling.
     *
     * All indices are in source frames. end is exclusive.
     * If end == 0 or end <= start, the region is treated as "no loop".
     */
    struct loop_region_t
    {
        /** Loop start frame (inclusive). */
        uint32_t start{ 0 };

        /** Loop end frame (exclusive); 0 = disabled. */
        uint32_t end{ 0 };
    };

    /**
     * @brief Configuration and runtime state for a single resampled frame.
     *
     * This is filled by the caller and passed to the resampler.
     * The resampler:
     *  - reads data / total_frames / channels / loop / looping / src_step
     *  - reads & *updates* src_pos
     */
    struct resample_request_t
    {
        /** Interleaved source PCM buffer (float32). */
        const float* data{ nullptr };

        /** Total number of source frames. */
        uint32_t total_frames{ 0 };

        /** Number of channels per frame (1 = mono, 2 = stereo). */
        uint32_t channels{ 0 };

        /** Current position in source frames (fractional). Updated by resampler. */
        double src_pos{ 0.0 };

        /** Step in source frames per output frame. */
        double src_step{ 1.0 };

        /** Whether to loop within the specified region. */
        bool looping{ false };

        /** Loop region in source frames. Ignored if looping == false. */
        loop_region_t loop{ };
    };

    /**
     * @brief Compute resampling step in source frames per engine frame.
     *
     * @param src_sample_rate  Sample rate of source data.
     * @param engine_sample_rate Engine mix rate (e.g. 48000).
     * @param pitch            Pitch multiplier (1.0 = normal).
     */
    inline double compute_resample_step(const double src_sample_rate, const double engine_sample_rate,
                                        const float pitch = 1.f) noexcept
    {
        return src_sample_rate / engine_sample_rate * static_cast<double>(pitch);
    }

    /**
     * @brief Linear interpolation helper for mono/stereo source data.
     *
     * This function:
     *  - Uses a fractional src_pos cursor in source frames,
     *  - Applies optional loop wrapping,
     *  - Produces a single stereo output frame via linear interpolation,
     *  - Advances src_pos by src_step.
     *
     * Ownership of "what happens at end of stream / loop" is left to the
     * caller, via the return value and updated src_pos.
     *
     * @param req Resampling configuration and state. src_pos is updated.
     * @param out_l [out] left sample.
     * @param out_r [out] right sample.
     *
     * @return true if a valid sample was produced, false if we've gone past
     *         the logical end in non-looping mode and should go silent.
     *
     * @note Real-time safe: no allocation, no locks.
     */
    inline bool resample_linear_frame(resample_request_t& req, float& out_l, float& out_r) noexcept
    {
        out_l = 0.f;
        out_r = 0.f;

        const float* data{ req.data };
        const uint32_t total_frames{ req.total_frames };
        const uint32_t channels{ req.channels };

        if (!data || total_frames == 0 || channels == 0)
            return false;

        // Effective loop bounds
        const uint32_t loop_start{ req.loop.start };
        uint32_t loop_end{ req.loop.end };

        if (!req.looping || loop_end == 0 || loop_end > total_frames || loop_end <= loop_start)
        {
            // Treat as no loop
            loop_end = total_frames;
        }

        const uint32_t sample_end{ loop_end }; // logical end
        double pos{ req.src_pos };

        // Non-looping: if we've gone past the end, call it done.
        if (!req.looping && pos >= static_cast<double>(sample_end))
            return false;

        // Clamp into [0, sample_end - 1] as a safety net.
        if (pos < 0.0) pos = 0.;
        if (pos > static_cast<double>(sample_end - 1))
            pos = static_cast<double>(sample_end - 1);

        if (!(pos == pos) || !std::isfinite(pos))
        {
            LOG_AUDIO_ERROR("NaN or Inf src_pos in resampler: {}", req.src_pos);
            req.src_pos = 0.0;
            return false;
        }

        // ── Loop-aware effective position ────────────────────────────────────
        double effective_pos{ pos };

        if (req.looping && loop_end > loop_start)
        {
            const double loop_start_d{ static_cast<double>(loop_start) };
            const double loop_end_d{ static_cast<double>(loop_end) };
            const double loop_len_d{ loop_end_d - loop_start_d };

            if (loop_len_d > 0.0)
            {
                if (pos >= loop_start_d)
                {
                    // Wrap into [loop_start, loop_end)
                    double base{ pos - loop_start_d };
                    base = std::fmod(base, loop_len_d);

                    if (base < 0.0)
                        base += loop_len_d;

                    effective_pos = loop_start_d + base;
                }
                else
                {
                    // Pre-loop region: 0..loop_start
                    effective_pos = pos;
                }
            }
        }
        else
        {
            // Non-looping or invalid loop: clamp inside playback region
            effective_pos = chlm::clamp(pos, 0.0, static_cast<double>(sample_end - 1));
        }

        const uint32_t i0{ static_cast<uint32_t>(effective_pos) };
        const uint32_t i1{ i0 + 1 < total_frames ? i0 + 1 : i0 };
        const double frac{ effective_pos - static_cast<double>(i0) };

        const uint32_t base0{ i0 * channels };
        const uint32_t base1{ i1 * channels };

        if (channels == 1)
        {
            const float s0{ data[base0 + 0] };
            const float s1{ data[base1 + 0] };
            const float v{
                static_cast<float>(static_cast<double>(s0) + (static_cast<double>(s1) - static_cast<double>(s0)) * frac)
            };

            out_l = v;
            out_r = v;
        }
        else // channels >= 2; use first two channels for L/R
        {
            const float s0_l{ data[base0 + 0] };
            const float s0_r{ data[base0 + 1] };
            const float s1_l{ data[base1 + 0] };
            const float s1_r{ data[base1 + 1] };

            out_l = static_cast<float>(static_cast<double>(s0_l) + (
                                           static_cast<double>(s1_l) - static_cast<double>(s0_l)) * frac);

            out_r = static_cast<float>(static_cast<double>(s0_r) + (
                                           static_cast<double>(s1_r) - static_cast<double>(s0_r)) * frac);
        }

        // ── Advance src_pos in time ─────────────────────────────────────────
        req.src_pos += req.src_step;

        if (req.looping && loop_end > loop_start)
        {
            const double loop_start_d{ static_cast<double>(loop_start) };
            const double loop_end_d{ static_cast<double>(loop_end) };
            const double loop_len_d{ loop_end_d - loop_start_d };

            if (loop_len_d > 0.0)
            {
                // Keep src_pos from drifting off to infinity while looping.
                double base{ req.src_pos - loop_start_d };
                base = std::fmod(base, loop_len_d);

                if (base < 0.0)
                    base += loop_len_d;

                req.src_pos = loop_start_d + base;
            }
        }

        // Non-looping: we let the caller decide what to do when src_pos >= sample_end.
        return true;
    }
} // namespace carrot::audio
