//
// Created by Zack Shrout on 2/25/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "DspUnit.h"

#include <cmath>
#include <cstdint>

namespace carrot::audio {
    /**
     * @enum biquad_type
     * @brief Enumerates the different types of biquad filters supported.
     *
     * This enumeration defines the various filter types that can be used
     * with a biquad filter implementation. These include:
     * - lowpass: Attenuates frequencies above a specified cutoff frequency.
     * - highpass: Attenuates frequencies below a specified cutoff frequency.
     * - bandpass: Passes frequencies within a specified range and attenuates frequencies outside of this range.
     * - notch: Rejects a narrow band of frequencies around a center frequency.
     * - peak: Boosts or attenuates frequencies around a center frequency.
     * - lowshelf: Boosts or attenuates frequencies below a specified frequency.
     * - highshelf: Boosts or attenuates frequencies above a specified frequency.
     */
    enum class biquad_type : uint8_t
    {
        lowpass,
        highpass,
        bandpass,
        notch,
        peak,
        lowshelf,
        highshelf
    };

    /**
     * @struct biquad_coeffs_t
     * @brief Stores coefficients for a biquad filter.
     *
     * This structure represents the coefficients used in the implementation
     * of a biquad filter. These coefficients define the transfer function
     * of the filter and dictate its behavior in terms of frequency response.
     *
     * The coefficients are:
     * - b0, b1, b2: Numerator coefficients for the filter.
     * - a1, a2: Denominator coefficients for the filter.
     *
     * Coefficients are typically normalized (divided by a0, where a0 is implicitly 1.0)
     * to simplify calculations during filter processing. The structure is initialized
     * with default values representing a pass-through filter.
     */
    struct biquad_coeffs_t
    {
        float b0{ 1.f };
        float b1{ 0.f };
        float b2{ 0.f };
        float a1{ 0.f };
        float a2{ 0.f };
    };

    /**
     * @class dsp_biquad_filter_t
     * @brief A DSP unit implementing a biquad filter for audio signal processing.
     *
     * This class provides a digital biquad filter implementation, supporting
     * various filter types including lowpass, highpass, bandpass, notch, peak,
     * lowshelf, and highshelf filters. The filter processes audio data and updates
     * its coefficients based on the specified parameters.
     */
    class dsp_biquad_filter_t final : public dsp_unit_t
    {
    public:
        enum param_id : uint32_t
        {
            param_freq = 0,
            param_q = 1,
            param_gain = 2 // only used for peak/lowshelf/highshelf
        };

        dsp_biquad_filter_t(const biquad_type type, const uint32_t sample_rate) : _type(type), _sample_rate(sample_rate)
        {
            update_coeffs();
            reset(sample_rate);
        }

        void process(dsp_process_context_t& ctx) noexcept override
        {
            float* const buffer{ ctx.interleaved };
            const uint32_t frames{ ctx.num_frames };
            const uint32_t channels{ ctx.num_channels };

            const float b0{ _coeffs.b0 };
            const float b1{ _coeffs.b1 };
            const float b2{ _coeffs.b2 };
            const float a1{ _coeffs.a1 };
            const float a2{ _coeffs.a2 };

            for (uint32_t f{ 0 }; f < frames; ++f)
            {
                for (uint32_t ch{ 0 }; ch < channels; ++ch)
                {
                    const uint32_t i{ f * channels + ch };

                    const float x{ buffer[i] };
                    const float y{ b0 * x + b1 * _z1[ch] + b2 * _z2[ch] - a1 * _z1[ch] - a2 * _z2[ch] };

                    _z2[ch] = _z1[ch];
                    _z1[ch] = y;

                    buffer[i] = y;
                }
            }
        }

        void set_parameter(const uint32_t id, const float value) noexcept override
        {
            switch (id)
            {
                case param_freq:
                    _freq = value;
                    update_coeffs();
                    break;

                case param_q:
                    _q = value;
                    update_coeffs();
                    break;

                case param_gain:
                    _gain_db = value;
                    update_coeffs();
                    break;
            }
        }

        void reset(const uint32_t sample_rate) noexcept override
        {
            _sample_rate = sample_rate;

            for (float& z: _z1) z = 0.f;
            for (float& z: _z2) z = 0.f;

            update_coeffs();
        }

        /**
         * @brief Sets the cutoff or center frequency of the biquad filter.
         *
         * This method updates the frequency parameter of the biquad filter, which
         * determines the cutoff frequency for lowpass and highpass filters, the
         * center frequency for bandpass and notch filters, or the affected frequency
         * for shelving and peak filters.
         *
         * The specified frequency value is internally stored and used in the calculation
         * of biquad filter coefficients. The value must be within a valid range based on
         * the sample rate of the signal being processed.
         *
         * @param f The frequency in Hertz (Hz) to set for the filter. This value must be
         *          greater than 0 and below half of the current sample rate (Nyquist frequency).
         */
        void set_freq(const float f) noexcept { set_parameter(param_freq, f); }

        /**
         * @brief Sets the Q-factor (quality factor) of the biquad filter.
         *
         * This method updates the Q parameter of the biquad filter, which affects
         * the resonance or bandwidth of the filter. The Q-factor is a dimensionless
         * parameter used to define the sharpness of the filter's frequency response,
         * especially around its cutoff or center frequency. A higher Q-factor
         * indicates a narrower bandwidth, while a lower Q-factor results in a wider
         * bandwidth.
         *
         * @param q The Q-factor value to set for the filter. This value must be
         *          greater than 0 and is typically a positive floating-point number.
         */
        void set_q(const float q) noexcept { set_parameter(param_q, q); }

        /**
         * @brief Sets the gain of the biquad filter.
         *
         * This method updates the gain parameter of the biquad filter, which is used
         * to boost or attenuate the amplitude for specific filter types such as peak or
         * shelving filters. The gain is specified in decibels (dB) and directly affects
         * the filter's behavior by changing its amplification or attenuation at the
         * specified frequency range.
         *
         * @param db The gain in decibels to set for the filter. Positive values increase
         *           the gain (boost), while negative values decrease it (attenuation).
         */
        void set_gain(const float db) noexcept { set_parameter(param_gain, db); }

    private:
        /**
         * @brief Updates the coefficients of the biquad filter based on the current settings.
         *
         * This method computes the biquad filter coefficients for different filter types
         * using the current filter type, frequency, Q-factor, gain, and sample rate.
         * The coefficients are normalized before being stored in the internal structure.
         *
         * The following filter types are supported:
         * - lowpass: Attenuates frequencies above the cutoff frequency.
         * - highpass: Attenuates frequencies below the cutoff frequency.
         * - bandpass: Passes frequencies within a specified range, attenuating others.
         * - notch: Rejects a narrow band of frequencies around a center frequency.
         * - peak: Boosts or attenuates frequencies around a center frequency.
         * - lowshelf: Boosts or attenuates frequencies below a specified frequency.
         * - highshelf: Boosts or attenuates frequencies above a specified frequency.
         *
         * Internally, the method computes intermediate values such as frequency in radians
         * (omega), sine and cosine components of omega, and other filter parameters such as
         * the gain factor (A) for specific types like shelving or peaking filters. Coefficients
         * are computed and normalized based on the selected filter type.
         *
         * @note This method recalculates the coefficients each time it is called, so
         * care should be taken to call it only when there's a change in filter parameters.
         */
        void update_coeffs() noexcept
        {
            const float omega{ 2.f * 3.141592653589f * (_freq / static_cast<float>(_sample_rate)) };
            const float sn{ std::sin(omega) };
            const float cs{ std::cos(omega) };
            const float alpha{ sn / (2.f * _q) };
            const float A{ std::pow(10.f, _gain_db / 40.f) }; // for shelves/peaking

            float b0{ 0.f }, b1{ 0.f }, b2{ 0.f }, a0{ 1.f }, a1{ 0.f }, a2{ 0.f };

            switch (_type)
            {
                case biquad_type::lowpass:
                    b0 = (1 - cs) * 0.5f;
                    b1 = 1 - cs;
                    b2 = (1 - cs) * 0.5f;
                    a0 = 1 + alpha;
                    a1 = -2 * cs;
                    a2 = 1 - alpha;
                    break;

                case biquad_type::highpass:
                    b0 = (1 + cs) * 0.5f;
                    b1 = -(1 + cs);
                    b2 = (1 + cs) * 0.5f;
                    a0 = 1 + alpha;
                    a1 = -2 * cs;
                    a2 = 1 - alpha;
                    break;

                case biquad_type::bandpass:
                    b0 = sn * 0.5f;
                    b1 = 0.f;
                    b2 = -sn * 0.5f;
                    a0 = 1 + alpha;
                    a1 = -2 * cs;
                    a2 = 1 - alpha;
                    break;

                case biquad_type::notch:
                    b0 = 1.f;
                    b1 = -2 * cs;
                    b2 = 1.f;
                    a0 = 1 + alpha;
                    a1 = -2 * cs;
                    a2 = 1 - alpha;
                    break;

                case biquad_type::peak:
                    b0 = 1 + alpha * A;
                    b1 = -2 * cs;
                    b2 = 1 - alpha * A;
                    a0 = 1 + alpha / A;
                    a1 = -2 * cs;
                    a2 = 1 - alpha / A;
                    break;

                case biquad_type::lowshelf:
                {
                    const float two_sqrtA_alpha{ 2 * std::sqrt(A) * alpha };
                    b0 = A * (A + 1 - (A - 1) * cs + two_sqrtA_alpha);
                    b1 = 2 * A * (A - 1 - (A + 1) * cs);
                    b2 = A * (A + 1 - (A - 1) * cs - two_sqrtA_alpha);
                    a0 = A + 1 + (A - 1) * cs + two_sqrtA_alpha;
                    a1 = -2 * (A - 1 + (A + 1) * cs);
                    a2 = A + 1 + (A - 1) * cs - two_sqrtA_alpha;
                }
                break;

                case biquad_type::highshelf:
                {
                    const float two_sqrtA_alpha{ 2 * std::sqrt(A) * alpha };
                    b0 = A * (A + 1 + (A - 1) * cs + two_sqrtA_alpha);
                    b1 = -2 * A * (A - 1 + (A + 1) * cs);
                    b2 = A * (A + 1 + (A - 1) * cs - two_sqrtA_alpha);
                    a0 = A + 1 - (A - 1) * cs + two_sqrtA_alpha;
                    a1 = 2 * (A - 1 - (A + 1) * cs);
                    a2 = A + 1 - (A - 1) * cs - two_sqrtA_alpha;
                }
                break;
            }

            // normalize
            _coeffs.b0 = b0 / a0;
            _coeffs.b1 = b1 / a0;
            _coeffs.b2 = b2 / a0;
            _coeffs.a1 = a1 / a0;
            _coeffs.a2 = a2 / a0;
        }

        biquad_type _type;
        biquad_coeffs_t _coeffs;

        float _freq{ 1000.0f };
        float _q{ 0.707f };
        float _gain_db{ 0.0f };

        uint32_t _sample_rate;

        // per-channel state
        float _z1[2]{ 0.0f, 0.0f };
        float _z2[2]{ 0.0f, 0.0f };
    };
} // namespace carrot::audio
