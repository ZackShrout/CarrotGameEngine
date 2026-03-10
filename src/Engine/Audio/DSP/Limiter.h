//
// Created by Zack Shrout on 2/26/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "DspUnit.h"

#include <cmath>
#include <algorithm>
#include <chlm/CarrotHLM.h>

namespace carrot::audio {
    /**
     * @class dsp_limiter_t
     * @brief A digital signal processing (DSP) unit for dynamic range limiting.
     *
     * This class implements a dynamic range limiter, which reduces the amplitude of audio signals
     * that exceed a specified threshold, ensuring that the output level remains below a defined ceiling.
     * The limiter also includes a configurable release time that controls how quickly the gain envelope
     * recovers after the signal drops below the threshold. The class is designed to process interleaved
     * audio data and supports configurable parameters for fine-tuning its behavior.
     */
    class dsp_limiter_t final : public dsp_unit_t
    {
    public:
        /**
         * @enum param_id
         * @brief Identifiers for configurable parameters of the DSP limiter.
         *
         * These identifiers are used to set or retrieve specific parameters for the
         * dynamic range limiter, allowing precise control over its behavior.
         */
        enum param_id : dsp_param_id_t
        {
            /** The signal level above which limiting is applied, expressed as a linear value (e.g., 0.9). */
            param_threshold = 0, // linear (e.g., 0.9)

            /** The maximum output level after limiting, expressed as a linear value (e.g., 0.98). */
            param_ceiling = 1, // linear (e.g., 0.98)

            /** The time, in milliseconds, for the gain envelope to recover after limiting is no longer required. */
            param_release_ms = 2 // time for gain to recover
        };

        dsp_limiter_t() noexcept = default;

        void process(dsp_process_context_t& ctx) noexcept override
        {
            float* const buffer{ ctx.interleaved };
            float release_coeff{ 0.f };

            if (_release_ms > 0.f && ctx.sample_rate > 0)
            {
                const float release_samples{ _release_ms / 1000.f * static_cast<float>(ctx.sample_rate) };
                // exponential decay over release_samples to ~36%
                release_coeff = std::exp(-1.f / chlm::max(release_samples, 1.f));
            }

            for (uint32_t f{ 0 }; f < ctx.num_frames; ++f)
            {
                const uint32_t base{ f * ctx.num_channels };

                // Find peak magnitude in this frame across all channels
                float peak{ 0.f };
                for (uint32_t ch{ 0 }; ch < ctx.num_channels; ++ch)
                {
                    const float x{ buffer[base + ch] };
                    peak = chlm::max(peak, std::fabs(x));
                }

                // Compute target gain
                float target_gain{ 1.f };
                if (peak > _threshold && peak > 0.f)
                {
                    // scale peak down to threshold, then apply ceiling
                    const float needed{ _threshold / peak };
                    target_gain = needed * _ceiling;
                }

                // Smooth gain: attack fast (take min), release slow
                if (target_gain < _gain_envelope)
                {
                    // Attack: snap down immediately to avoid clipping
                    _gain_envelope = target_gain;
                }
                else
                {
                    // Release: exponential towards 1.0
                    if (release_coeff > 0.f)
                    {
                        _gain_envelope = _gain_envelope * release_coeff + (1.f - release_coeff) * 1.f;
                    }
                    else
                    {
                        _gain_envelope = 1.f;
                    }
                }

                // Apply gain to all channels in this frame
                for (uint32_t ch = 0; ch < ctx.num_channels; ++ch)
                    buffer[base + ch] *= _gain_envelope;
            }
        }

        void set_parameter(const uint32_t id, const float value) noexcept override
        {
            switch (id)
            {
                case param_threshold:
                    _threshold = chlm::clamp(value, 0.f, 1.f);
                    break;
                case param_ceiling:
                    _ceiling = chlm::clamp(value, 0.f, 1.5f);
                    break;
                case param_release_ms:
                    _release_ms = chlm::max(0.f, value);
                    break;
            }
        }

        void reset(uint32_t /*sample_rate*/) noexcept override { _gain_envelope = 1.f; }

        /**
         * @brief Sets the threshold level for the limiter.
         *
         * This method defines the signal level above which the limiter will begin to reduce the dynamic range.
         * The threshold is expressed as a linear value where 1.0 corresponds to the maximum level.
         *
         * @param t The threshold value, specified as a linear floating-point value (e.g., 0.9 for 90% of the maximum level).
         */
        void set_threshold(const float t) noexcept { set_parameter(param_threshold, t); }

        /**
         * @brief Sets the ceiling level for the limiter.
         *
         * This method specifies the maximum output level that the audio signal can reach after
         * applying dynamic range limiting. The ceiling is expressed as a linear value, where 1.0
         * represents the maximum possible level.
         *
         * @param c The ceiling value, specified as a linear floating-point value (e.g., 0.98 for 98% of the maximum level).
         */
        void set_ceiling(const float c) noexcept { set_parameter(param_ceiling, c); }

        /**
         * @brief Sets the release time for the limiter in milliseconds.
         *
         * This method defines the duration, in milliseconds, for the gain envelope
         * to recover to its original value after the signal level drops below
         * the limiting threshold. A longer release time results in smoother recovery,
         * while a shorter release time allows quicker gain recovery.
         *
         * @param r The release time in milliseconds, specified as a floating-point value.
         */
        void set_release_ms(const float r) noexcept { set_parameter(param_release_ms, r); }

    private:
        /**
         * @var _threshold
         * @brief The signal level above which dynamic range limiting is applied.
         *
         * This variable defines the threshold level for the limiter, expressed as a linear floating-point value.
         * When the signal's peak amplitude exceeds this value, the limiter engages to reduce the amplitude.
         * A value of 1.0 represents the maximum possible signal level, while lower values define a more aggressive
         * limiting threshold.
         */
        float _threshold{ 0.9f };

        /**
         * @var _ceiling
         * @brief The maximum output level after dynamic range limiting.
         *
         * This variable defines the ceiling level for the limiter, expressed as a linear floating-point value.
         * The ceiling ensures that the audio signal's maximum amplitude does not exceed the specified value
         * after the limiting process is applied. A typical value is slightly below 1.0 (e.g., 0.98), allowing
         * a small margin below the absolute maximum signal level.
         */
        float _ceiling{ 0.98f };

        /**
         * @var _release_ms
         * @brief The release time for the dynamic range limiter, in milliseconds.
         *
         * This variable specifies the duration required for the limiter's gain envelope
         * to return to its normal level after the signal level falls below the limiting threshold.
         * A higher value results in a smoother and slower release, while a lower value allows
         * for quicker recovery. The release time directly influences the perceived smoothness and
         * naturalness of the limiter's behavior.
         */
        float _release_ms{ 100.f };

        /**
         * @var _gain_envelope
         * @brief Current gain adjustment applied by the dynamic range limiter.
         *
         * This variable represents the current state of the gain envelope, which is used
         * to dynamically adjust the amplitude of the audio signal based on the limiter's
         * parameters and input signal characteristics. The value of this variable is typically
         * computed in real-time and reflects the instantaneous gain reduction or recovery
         * applied to ensure the output remains within the specified threshold and ceiling levels.
         * It starts with an initial value of 1.0, indicating no gain adjustment.
         */
        float _gain_envelope{ 1.f };
    };
} // namespace carrot::audio
