//
// Created by Zack Shrout on 2/26/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "DspUnit.h"
#include "BiquadFilter.h"

#include <algorithm>
#include <chlm/CarrotHLM.h>

namespace carrot::audio {
    /**
     * @class dsp_baxandall_tone_t
     * @brief Implements a Baxandall tone control unit for digital signal processing.
     *
     * This class provides a two-band Baxandall tone control with adjustable bass
     * and treble levels. It utilizes biquad filters to implement low-shelf and
     * high-shelf filters for signal processing. Designed for real-time audio
     * applications, it is configurable with adjustable frequency and gain for each band.
     */
    class dsp_baxandall_tone_t final : public dsp_unit_t
    {
    public:
        /**
         * @enum param_id
         * @brief Enumerates parameter IDs used to control the Baxandall tone control unit.
         *
         * This enum defines the identifiers for various adjustable parameters of the
         * Baxandall tone control unit. These parameters are used to configure
         * the gain and frequency settings for the bass and treble bands.
         */
        enum param_id : dsp_param_id_t
        {
            /** Controls the bass gain in decibels, ranging from -5 to +5 dB. */
            param_bass_gain_db = 0,

            /** Controls the treble gain in decibels, ranging from -5 to +5 dB. */
            param_treble_gain_db = 1,

            /** Sets the frequency for the bass shelf filter (e.g. 100 Hz). */
            param_bass_freq_hz = 2,

            /** Sets the frequency for the treble shelf filter (e.g. 8000 Hz). */
            param_treble_freq_hz = 3,

            /** Sets the frequency for the high-pass filter, ranging from 12 Hz to 54 Hz. */
            param_hpf_freq_hz = 4,

            /** Sets the frequency for the low-pass filter, ranging from 7.5 kHz to 70 kHz. */
            param_lpf_freq_hz = 5,
        };

        explicit dsp_baxandall_tone_t(const uint32_t sample_rate) noexcept
            : _sample_rate(sample_rate)
              , _low_shelf_1(biquad_type::lowshelf, sample_rate), _low_shelf_2(biquad_type::lowshelf, sample_rate)
              , _high_shelf_1(biquad_type::highshelf, sample_rate), _high_shelf_2(biquad_type::highshelf, sample_rate)
              , _hpf(biquad_type::highpass, sample_rate), _lpf(biquad_type::lowpass, sample_rate)
        {
            update_filters();
        }

        void process(dsp_process_context_t& ctx) noexcept override
        {
            if (!ctx.interleaved || ctx.num_channels == 0 || ctx.num_frames == 0)
                return;

            if (_hpf_enabled)
                _hpf.process(ctx);

            _low_shelf_1.process(ctx);
            _low_shelf_2.process(ctx);
            _high_shelf_1.process(ctx);
            _high_shelf_2.process(ctx);

            if (_lpf_enabled)
                _lpf.process(ctx);
        }

        void set_parameter(const uint32_t id, const float value) noexcept override
        {
            switch (id)
            {
                case param_bass_gain_db:
                {
                    constexpr float min_gain{ -5.f };
                    constexpr float max_gain{ 5.f };
                    _bass_gain_db = chlm::clamp(value, min_gain, max_gain);

                    break;
                }

                case param_treble_gain_db:
                {
                    constexpr float min_gain{ -5.f };
                    constexpr float max_gain{ 5.f };
                    _treble_gain_db = chlm::clamp(value, min_gain, max_gain);

                    break;
                }

                case param_bass_freq_hz:
                {
                    constexpr float min_bass{ 74.f };
                    constexpr float max_bass{ 361.f };
                    _bass_freq_hz = chlm::clamp(value, min_bass, max_bass);

                    break;
                }

                case param_treble_freq_hz:
                {
                    constexpr float min_treble{ 1600.f };
                    constexpr float max_treble{ 18000.f };
                    _treble_freq_hz = chlm::clamp(value, min_treble, max_treble);

                    break;
                }

                case param_hpf_freq_hz:
                {
                    if (value <= 0.f)
                    {
                        _hpf_freq_hz = 0.f;
                        _hpf_enabled = false;

                        break;
                    }

                    constexpr float min_hpf{ 12.f };
                    constexpr float max_hpf{ 54.f };
                    _hpf_enabled = true;
                    _hpf_freq_hz = chlm::clamp(value, min_hpf, max_hpf);

                    break;
                }

                case param_lpf_freq_hz:
                {
                    if (value <= 0.0f)
                    {
                        _lpf_freq_hz = 0.f;
                        _lpf_enabled = false;
                        update_filters();

                        return;
                    }

                    constexpr float min_lpf{ 7500.f };
                    constexpr float max_lpf{ 70000.f };
                    _lpf_enabled = true;
                    _lpf_freq_hz = chlm::clamp(value, min_lpf, max_lpf);

                    break;
                }
            }

            update_filters();
        }

        void reset(const uint32_t sample_rate) noexcept override
        {
            _sample_rate = sample_rate;
            _low_shelf_1.reset(sample_rate);
            _low_shelf_2.reset(sample_rate);
            _high_shelf_1.reset(sample_rate);
            _high_shelf_2.reset(sample_rate);
            _hpf.reset(sample_rate);
            _lpf.reset(sample_rate);
            update_filters();
        }

        /**
         * @brief Sets the gain for the bass band in decibels.
         *
         * Adjusts the gain for the bass band in the Baxandall tone control unit. This method
         * allows for boosting or attenuating the low-frequency content of the audio signal.
         * The input gain value is clamped to a fixed range, consistent with the behavior of
         * hardware Baxandall units.
         *
         * @param db The desired bass gain in decibels. The value is clamped to the range [-5.0, 5.0].
         */
        void set_bass_gain_db(const float db) noexcept { set_parameter(param_bass_gain_db, db); }

        /**
         * @brief Sets the gain for the treble band in decibels.
         *
         * This method adjusts the gain for the treble band in the Baxandall tone control unit.
         * It allows for boosting or attenuating the high-frequency content of the audio signal.
         * The input gain value is clamped to a fixed range that mimics the behavior
         * of hardware Baxandall units.
         *
         * @param db The desired treble gain in decibels. The value is clamped to the range [-5.0, 5.0].
         */
        void set_treble_gain_db(const float db) noexcept { set_parameter(param_treble_gain_db, db); }

        /**
         * @brief Sets the frequency for the bass shelf filter.
         *
         * This method adjusts the cutoff frequency of the bass band in the Baxandall
         * tone control unit. It defines the point at which the bass shelf filter begins
         * to affect the signal, shaping the low-frequency content of the audio.
         * The given frequency is clamped within the permitted range, which is designed
         * to mimic the behavior of hardware Baxandall units.
         *
         * @param hz The desired bass frequency in hertz. The value is clamped
         *           to the range [74.0, 361.0] Hz.
         */
        void set_bass_freq(const float hz) noexcept { set_parameter(param_bass_freq_hz, hz); }

        /**
         * @brief Sets the frequency for the treble shelf filter.
         *
         * This method adjusts the cutoff frequency of the treble band in the Baxandall
         * tone control unit. It defines the point at which the treble shelf filter begins
         * to affect the signal, shaping the high-frequency content of the audio.
         * The given frequency is clamped to ensure it stays within the permitted range,
         * which mimics the behavior of traditional hardware Baxandall units.
         *
         * @param hz The desired treble frequency in hertz. The value is clamped
         *           to the range [1600.0, 18000.0] Hz.
         */
        void set_treble_freq(const float hz) noexcept { set_parameter(param_treble_freq_hz, hz); }

        /**
         * @brief Sets the high-pass filter (HPF) cutoff frequency and enables or disables the HPF.
         *
         * Adjusts the cutoff frequency of the high-pass filter to a specified value within a
         * defined range. If the input frequency is set to a non-positive value, the HPF is
         * disabled. If the frequency is within the valid range, the HPF is enabled, and the
         * internal filters are updated accordingly.
         *
         * @param hz The desired HPF cutoff frequency in Hertz. Must be greater than 0 to enable
         *           the filter. Valid frequency range for the HPF is clamped between 12 Hz and
         *           54 Hz.
         */
        void set_hpf_freq(const float hz) noexcept { set_parameter(param_hpf_freq_hz, hz); }

        /**
         * @brief Sets the cutoff frequency for the low-pass filter.
         *
         * This method configures the low-pass filter (LPF) with the specified cutoff
         * frequency, clamping it to the range of 7.5 kHz to 70 kHz. A frequency of
         * 0 or below disables the filter.
         *
         * @param hz The desired cutoff frequency in Hertz. Requires a positive value
         *           within the range of 7.5 kHz to 70 kHz. Any value outside this
         *           range is clamped. A value of 0 disables the filter.
         */
        void set_lpf_freq(const float hz) noexcept { set_parameter(param_lpf_freq_hz, hz); }

    private:
        uint32_t _sample_rate{ 48000 };

        float _bass_gain_db{ 0.f };
        float _treble_gain_db{ 0.f };
        float _bass_freq_hz{ 99.5f };
        float _treble_freq_hz{ 8944.f };

        float _hpf_freq_hz{ 0.f }; // 0 => off
        float _lpf_freq_hz{ 0.f }; // 0 => off
        bool _hpf_enabled{ false };
        bool _lpf_enabled{ false };

        static constexpr float k_bass_spread{ 3.27f }; // targets ~55/180 when center=100
        static constexpr float k_treble_spread{ 3.2f }; // targets ~5k/16k when center≈9k

        dsp_biquad_filter_t _low_shelf_1;
        dsp_biquad_filter_t _low_shelf_2;
        dsp_biquad_filter_t _high_shelf_1;
        dsp_biquad_filter_t _high_shelf_2;
        dsp_biquad_filter_t _hpf;
        dsp_biquad_filter_t _lpf;

        /**
         * @brief Updates the internal filter parameters for the Baxandall tone control unit.
         *
         * This method recalculates and applies the frequency, gain, and quality (Q) settings
         * for the low-shelf and high-shelf filters in the tone control unit. It divides the
         * bass and treble gain values evenly across two filter stages and configures each
         * stage with an appropriate frequency spread and Q factor.
         *
         * The bass and treble frequency ranges are adjusted based on a configurable
         * spread ratio to create a broader or more concentrated frequency response.
         * This ensures smooth, natural changes in tonal balance.
         */
        void update_filters() noexcept
        {
            const float half_bass_db{ _bass_gain_db * 0.5f };
            const float half_treble_db{ _treble_gain_db * 0.5f };

            const float bass_r{ std::sqrt(k_bass_spread) };
            const float treble_r{ std::sqrt(k_treble_spread) };

            const float bass_f1{ _bass_freq_hz / bass_r };
            const float bass_f2{ _bass_freq_hz * bass_r };

            const float treble_f1{ _treble_freq_hz / treble_r };
            const float treble_f2{ _treble_freq_hz * treble_r };

            if (_hpf_enabled && _hpf_freq_hz > 0.0f)
            {
                _hpf.set_freq(_hpf_freq_hz);
                _hpf.set_q(0.707f);
            }

            _low_shelf_1.set_freq(bass_f1);
            _low_shelf_1.set_q(0.5f);
            _low_shelf_1.set_gain(half_bass_db);

            _low_shelf_2.set_freq(bass_f2);
            _low_shelf_2.set_q(0.5f);
            _low_shelf_2.set_gain(half_bass_db);

            _high_shelf_1.set_freq(treble_f1);
            _high_shelf_1.set_q(0.5f);
            _high_shelf_1.set_gain(half_treble_db);

            _high_shelf_2.set_freq(treble_f2);
            _high_shelf_2.set_q(0.5f);
            _high_shelf_2.set_gain(half_treble_db);

            if (_lpf_enabled && _lpf_freq_hz > 0.0f)
            {
                _lpf.set_freq(_lpf_freq_hz);
                _lpf.set_q(0.707f); // Butterworth-ish, smooth
            }
        }
    };
} // namespace carrot::audio
