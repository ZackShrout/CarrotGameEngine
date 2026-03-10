//
// Created by Zack Shrout on 2/27/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "DspUnit.h"

#include <chlm/CarrotHLM.h>

namespace carrot::audio {
    /**
     * @enum vca_bus_comp_param
     * @brief Parameters for configuring the VCA bus compressor.
     *
     * This enumeration defines the adjustable parameters for a VCA bus-style compressor,
     * enabling fine control over compression behavior and tonal shaping.
     */
    enum class vca_bus_comp_param : dsp_param_id_t
    {
        /** */
        threshold_db = 0, // e.g. -24 .. 0

        /** */
        ratio, // e.g. 1.5 .. 10

        /** */
        knee_db, // 0 = hard knee, 6 = gentle, 12 = very soft

        /** */
        attack_ms, // 0.1 .. 30 ms

        /** */
        release_ms, // 50 .. 2000 ms (or 0 for "auto" later if you want)

        /** */
        makeup_gain_db, // -12 .. +12

        /** */
        detection_mode, // 0 = RMS, 1 = PEAK

        /** */
        mix, // 0..1 for parallel compression (optional, but tasty)
    };

    /**
     * @enum vca_comp_detection_mode_t
     * @brief Modes for signal detection in VCA compressors.
     *
     * Enumerates the methods used for detecting signal levels in a VCA compressor.
     * Detection can be based on either RMS (Root Mean Square) or peak levels,
     * which affects how compression is applied to the audio signal.
     */
    enum class vca_comp_detection_mode_t : uint32_t
    {
        rms = 0,
        peak = 1
    };

    /**
     * @class dsp_vca_bus_comp_t
     * @brief Implements a DSP unit for a bus-style VCA compressor with flexible parameters.
     *
     * This class provides a sidechain compression mechanism for audio signals,
     * featuring configurable attack, release, ratio, threshold, knee width,
     * makeup gain, detection mode (RMS or peak), and parallel mix. It employs
     * soft-knee compression as well as SSL/API-style presets for quick configuration.
     */
    class dsp_vca_bus_comp_t final : public dsp_unit_t
    {
    public:
        void process(dsp_process_context_t& ctx) noexcept override
        {
            if (!ctx.interleaved || ctx.num_channels == 0 || ctx.num_frames == 0)
                return;

            for (uint32_t n{ 0 }; n < ctx.num_frames; ++n)
            {
                float* frame{ ctx.interleaved + n * ctx.num_channels };

                // 1) Sidechain detection
                const float level_in{ compute_input_level(frame, ctx.num_channels) };
                update_detector(level_in);

                // 2) Compute level in dB
                const float level_db{ lin_to_db(_detector_env) };

                // 3) Gain reduction (negative or zero)
                const float target_gr_db{ compute_gain_reduction_db(level_db) };

                // 4) Smooth gain reduction over time
                update_gain_smooth(target_gr_db);

                // 5) Total gain = makeup + gain_reduction
                const float total_gain_db{ _makeup_gain_db + _gain_smooth_db };
                const float total_gain_lin{ db_to_lin(total_gain_db) };

                // 6) Apply (optionally parallel blend)
                if (_mix >= 0.999f)
                {
                    for (uint32_t ch{ 0 }; ch < ctx.num_channels; ++ch)
                        frame[ch] *= total_gain_lin;
                }
                else
                {
                    // simple parallel: (dry + wet*gain) blend
                    for (uint32_t ch{ 0 }; ch < ctx.num_channels; ++ch)
                    {
                        const float dry{ frame[ch] };
                        const float wet{ dry * total_gain_lin };
                        frame[ch] = dry * (1.0f - _mix) + wet * _mix;
                    }
                }
            }
        }

        void set_parameter(uint32_t id, const float value) noexcept override
        {
            switch (static_cast<vca_bus_comp_param>(id))
            {
                case vca_bus_comp_param::threshold_db:
                    _threshold_db = value;
                    break;
                case vca_bus_comp_param::ratio:
                    _ratio = value <= 1.0f ? 1.0f : value;
                    break;
                case vca_bus_comp_param::knee_db:
                    _knee_db = value < 0.0f ? 0.0f : value;
                    break;
                case vca_bus_comp_param::attack_ms:
                    _attack_ms = value;
                    _attack_coeff = std::exp(-1.0f / (_attack_ms * 0.001f * static_cast<float>(_sample_rate)));
                    break;
                case vca_bus_comp_param::release_ms:
                    _release_ms = value;
                    _release_coeff = std::exp(-1.0f / (_release_ms * 0.001f * static_cast<float>(_sample_rate)));
                    break;
                case vca_bus_comp_param::makeup_gain_db:
                    _makeup_gain_db = value;
                    break;
                case vca_bus_comp_param::detection_mode:
                    _mode = value >= 0.5f ? vca_comp_detection_mode_t::peak : vca_comp_detection_mode_t::rms;
                    break;
                case vca_bus_comp_param::mix:
                    _mix = chlm::clamp(value, 0.0f, 1.0f);
                    break;
            }
        }

        void reset(const uint32_t sample_rate) noexcept override
        {
            _sample_rate = sample_rate;

            const float attack_sec{ _attack_ms * 0.001f };
            const float release_sec{ _release_ms * 0.001f };

            _attack_coeff = std::exp(-1.f / (attack_sec * static_cast<float>(_sample_rate)));
            _release_coeff = std::exp(-1.f / (release_sec * static_cast<float>(_sample_rate)));

            _detector_env = db_to_lin(-90.f);
            _gain_smooth_db = 0.f;
        }

        /**
         * @brief Sets the threshold level for the VCA bus compressor in decibels.
         *
         * This method adjusts the threshold parameter, which defines the input signal level
         * at which compression begins. Values are typically in the range of -24 dB to 0 dB.
         * A lower threshold results in more signal being compressed, while a higher threshold
         * allows more of the input signal to pass unaffected.
         *
         * @param value The threshold level in decibels.
         */
        void set_threshold_db(const float value) noexcept
        {
            set_parameter(static_cast<uint32_t>(vca_bus_comp_param::threshold_db), value);
        }

        /**
         * @brief Sets the compression ratio for the VCA bus compressor.
         *
         * This method adjusts the ratio parameter of the VCA bus compressor, which
         * determines the amount of gain reduction applied to signals above the
         * threshold level. A higher ratio results in more aggressive compression.
         *
         * @param value The compression ratio. Typical values range from 1.5 to 10.
         */
        void set_ratio(const float value) noexcept
        {
            set_parameter(static_cast<uint32_t>(vca_bus_comp_param::ratio), value);
        }

        /**
         * @brief Sets the knee parameter for the VCA bus compressor in decibels.
         *
         * This method adjusts the knee parameter, which defines the transition between
         * no compression and full compression. A higher knee value results in a more
         * gradual transition, while a lower value creates a sharper or "harder" knee.
         *
         * @param value The knee value in decibels. Typical values are 0 (hard knee), 6 (gentle knee),
         *              and 12 (very soft knee).
         */
        void set_knee_db(const float value) noexcept
        {
            set_parameter(static_cast<uint32_t>(vca_bus_comp_param::knee_db), value);
        }

        /**
         * @brief Sets the attack time for the VCA bus compressor in milliseconds.
         *
         * This method adjusts the attack time parameter, which determines how quickly
         * the compressor responds to the input signal once it exceeds the threshold level.
         * A shorter attack time results in faster response, while a longer attack time
         * provides a smoother onset of compression.
         *
         * @param value The attack time in milliseconds. Typical values range from 0.1 ms to 30 ms.
         */
        void set_attack_ms(const float value) noexcept
        {
            set_parameter(static_cast<uint32_t>(vca_bus_comp_param::attack_ms), value);
        }

        /**
         * @brief Sets the release time for the VCA bus compressor in milliseconds.
         *
         * This method adjusts the release time parameter, which determines how quickly
         * the compressor stops reducing gain after the input signal falls below the
         * threshold level. A shorter release time provides faster recovery, while a
         * longer release time creates a smoother release of compression.
         *
         * @param value The release time in milliseconds. Typical values range from 20 ms to 2000 ms.
         */
        void set_release_ms(const float value) noexcept
        {
            set_parameter(static_cast<uint32_t>(vca_bus_comp_param::release_ms), value);
        }

        /**
         * @brief Sets the makeup gain for the VCA bus compressor in decibels.
         *
         * This method adjusts the makeup gain parameter, which is used to compensate
         * for the level reduction caused by the compression process. Increasing the
         * makeup gain restores the overall signal level after compression.
         *
         * @param value The makeup gain in decibels. Typical values range from 0 dB
         *              (no gain adjustment) to a positive value for level compensation.
         */
        void set_makeup_gain_db(const float value) noexcept
        {
            set_parameter(static_cast<uint32_t>(vca_bus_comp_param::makeup_gain_db), value);
        }

        /**
         * @brief Sets the detection mode for the VCA bus compressor.
         *
         * This method specifies the detection mode used by the VCA bus compressor to
         * analyze the input signal for compression. The detection mode determines how
         * the input signal is measured, influencing compression behavior.
         *
         * @param mode The detection mode to set. The mode is represented by the
         *             vca_comp_detection_mode_t enumeration.
         */
        void set_detection_mode(const vca_comp_detection_mode_t mode) noexcept
        {
            set_parameter(static_cast<uint32_t>(vca_bus_comp_param::detection_mode), static_cast<float>(mode));
        }

        /**
         * @brief Sets the mix parameter for the VCA bus compressor.
         *
         * This method adjusts the mix level for the VCA bus compressor, determining
         * the balance between the compressed and uncompressed signal.
         *
         * @param value The mix value to be set, typically ranging from 0.0 (fully dry)
         * to 1.0 (fully wet).
         */
        void set_mix(const float value) noexcept
        {
            set_parameter(static_cast<uint32_t>(vca_bus_comp_param::mix), value);
        }

        /**
         * @brief Configures the compressor to emulate the settings of an SSL-style bus compressor preset.
         *
         * This method applies a predefined set of parameters to achieve the sound characteristic of an SSL-style
         * bus compressor. It fine-tunes threshold, ratio, knee, attack, release, makeup gain, detection mode, and
         * wet/dry mix to provide a familiar compression profile.
         *
         * @note The method is noexcept, ensuring it does not throw exceptions.
         */
        void set_preset_ssl_style() noexcept
        {
            set_threshold_db(-4.f);
            set_ratio(2.f);
            set_knee_db(6.f);
            set_attack_ms(10.f);
            set_release_ms(300.f);
            set_makeup_gain_db(1.f);
            set_detection_mode(vca_comp_detection_mode_t::rms);
            set_mix(1.f);
        }

        /**
         * @brief Configures the compressor with a preset API-style setting.
         *
         * This method applies a predefined configuration to the compressor,
         * optimizing it for a specific API use case. The preset adjusts
         * threshold, ratio, knee, attack, release, makeup gain, detection
         * mode, and mix levels to achieve the desired behavior.
         *
         * @note This configuration provides a standardized setup and may
         * require further adjustment based on specific audio processing needs.
         */
        void set_preset_api_style() noexcept
        {
            set_threshold_db(-6.f);
            set_ratio(3.f);
            set_knee_db(3.f);
            set_attack_ms(30.f);
            set_release_ms(100.f);
            set_makeup_gain_db(1.f);
            set_detection_mode(vca_comp_detection_mode_t::peak);
            set_mix(0.7f);
        }

    private:
        /**
         * @brief Computes the input signal level based on the specified detection mode.
         *
         * This method determines the input level of an audio frame by analyzing the signal
         * across multiple channels. Depending on the currently selected detection mode,
         * the level is calculated using either the peak value or the RMS (root mean square).
         *
         * @param frame A pointer to the audio frame buffer containing the per-channel sample values.
         * @param num_channels The number of channels in the audio frame.
         * @return The computed input level as a floating-point value, representing either the peak
         *         or RMS value of the input signal based on the detection mode.
         */
        float compute_input_level(const float* frame, const uint32_t num_channels) const noexcept
        {
            if (_mode == vca_comp_detection_mode_t::peak)
            {
                float peak{ 0.f };

                for (uint32_t ch{ 0 }; ch < num_channels; ++ch)
                    peak = chlm::max(peak, std::fabs(frame[ch]));

                return peak;
            }

            float sum_sq{ 0.f };

            for (uint32_t ch{ 0 }; ch < num_channels; ++ch)
                sum_sq += frame[ch] * frame[ch];

            const float mean_sq{ sum_sq / static_cast<float>(num_channels) };

            return chlm::sqrt(mean_sq + 1e-20f);
        }

        /**
         * @brief Updates the detector envelope based on the input level.
         *
         * This method processes the input signal level and updates the detector envelope
         * using either the attack or release coefficient, depending on the relationship
         * between the input level and the current envelope value. It can be used to model
         * dynamic response in audio signal processing such as compressors or limiters.
         *
         * @param level_in The input signal level, typically represented as a floating-point
         *                 value. It may correspond to linear amplitude or power, depending
         *                 on the implementation.
         */
        void update_detector(const float level_in) noexcept
        {
            // For RMS-as-power version:
            // float x = level_in; // squared
            // For simple version, treat as linear amplitude

            if (level_in > _detector_env)
                _detector_env = _attack_coeff * _detector_env + (1.f - _attack_coeff) * level_in;
            else
                _detector_env = _release_coeff * _detector_env + (1.f - _release_coeff) * level_in;
        }

        /**
         * @brief Computes the amount of gain reduction in decibels (dB) applied by the compressor based on the input signal level.
         *
         * This method calculates the gain reduction according to the threshold, ratio, and knee parameters
         * of the VCA bus compressor. It supports both hard knee and soft knee compression modes, with smooth
         * interpolation within the knee region for soft knee compression.
         *
         * @param level_db The input signal level in decibels (dB).
         * @return The calculated gain reduction in decibels (dB).
         */
        [[nodiscard]] float compute_gain_reduction_db(const float level_db) const noexcept
        {
            // Let:
            //     L = level_db
            //     T = _threshold_db
            //     R = _ratio
            //     K = _knee_db (width in dB; 0 = hard knee)
            // Standard soft knee:
            //     If L <= T - K/2 → no compression (gain_red_db = 0)
            //     If L >= T + K/2 → hard knee region
            //     over_db = L - T
            //     gain_red_db = (over_db) * (1.0f - 1.0f / R)
            // In-between, within knee width: we interpolate smoothly

            if (_knee_db <= 0.f)
            {
                // hard knee
                if (level_db <= _threshold_db)
                    return 0.f;

                const float over_db{ level_db - _threshold_db };

                return over_db * (1.f - 1.f / _ratio);
            }

            const float lower{ _threshold_db - _knee_db * 0.5f };
            const float upper{ _threshold_db + _knee_db * 0.5f };

            if (level_db <= lower)
                return 0.f;

            if (level_db >= upper)
            {
                const float over_db{ level_db - _threshold_db };

                return over_db * (1.f / _ratio - 1.f);
            }

            // In the knee region: smooth quadratic transition
            const float x{ level_db - lower }; // 0 .. K
            const float frac{ x / _knee_db }; // 0 .. 1

            // "target" hard-knee gain reduction at this level:
            const float over_db{ level_db - _threshold_db };
            const float hard_gr{ over_db * (1.f / _ratio - 1.f) };

            // cubic smoothstep: 0..1 with zero slope at both ends
            const float t{ frac * frac * (3.f - 2.f * frac) };

            // fade in from 0 to hard_gr using a gentle curve (frac^2 is fine)
            return hard_gr * t;
        }

        /**
         * @brief Updates the smoothed gain reduction value based on the target gain reduction.
         *
         * This method adjusts the internal smoothed gain reduction value. Depending on whether
         * the target gain reduction is greater or less than the current smoothed value, the method
         * applies either an attack or a release coefficient to determine the rate of change.
         *
         * @param target_gr_db The target gain reduction in decibels (negative or zero).
         */
        void update_gain_smooth(const float target_gr_db) noexcept
        {
            // _gain_smooth_db is negative or zero
            if (target_gr_db < _gain_smooth_db) // more reduction (more negative)
            {
                // attack (tighten quickly)
                _gain_smooth_db = _attack_coeff * _gain_smooth_db + (1.f - _attack_coeff) * target_gr_db;
            }
            else
            {
                // release (back off slowly)
                _gain_smooth_db = _release_coeff * _gain_smooth_db + (1.f - _release_coeff) * target_gr_db;
            }
        }

        /**
         * @brief Converts a linear amplitude value to its corresponding value in decibels (dB).
         *
         * This function calculates the decibel (dB) equivalent of a linear amplitude value.
         * The formula includes a small offset to prevent logarithmic singularity at zero.
         *
         * @param x The linear amplitude value to be converted. Must be a non-negative float.
         * @return The decibel representation of the input linear value.
         */
        float lin_to_db(const float x) noexcept { return 20.f * std::log10(x + 1e-20f); }

        /**
         * @brief Converts a value in decibels (dB) to its linear scale equivalent.
         *
         * This function transforms a decibel value into the linear scale for use in
         * audio processing and other signal-related calculations.
         *
         * @param db The value in decibels to be converted.
         * @return The linear scale equivalent of the decibel value.
         */
        float db_to_lin(const float db) noexcept { return std::pow(10.f, db * 0.05f); }

        uint32_t _sample_rate{ 48000 };
        vca_comp_detection_mode_t _mode{ vca_comp_detection_mode_t::rms };

        // raw parameters
        float _threshold_db{ -2.f };
        float _ratio{ 2.f };
        float _knee_db{ 6.f };
        float _attack_ms{ 10.f };
        float _release_ms{ 300.f };
        float _makeup_gain_db{ 1.f };
        float _mix{ 1.f };

        // computed coefficients
        float _attack_coeff{ 0.f };
        float _release_coeff{ 0.f };

        // state
        float _detector_env{ 0.f }; // linear level (not dB)
        float _gain_smooth_db{ 0.f }; // smoothed attenuation (<= 0)
    };
} // namespace carrot::audio
