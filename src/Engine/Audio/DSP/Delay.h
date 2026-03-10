//
// Created by Zack Shrout on 2/25/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "DspUnit.h"
#include "chlm/CarrotHLM.h"

#include <algorithm>

namespace carrot::audio {
    /**
     * @class dsp_delay_line_t
     * @brief A DSP unit that implements a delay line with configurable parameters.
     *
     * The `dsp_delay_line_t` class provides a digital delay line effect with adjustable
     * delay time, feedback, wet/dry mix, and support for multi-channel audio. It is implemented
     * as a circular buffer to achieve efficient memory management.
     */
    class dsp_delay_line_t final : public dsp_unit_t
    {
    public:
        /**
         * @enum param_id
         * @brief Identifiers for configurable parameters of the delay line DSP unit.
         *
         * This enumeration defines the parameter IDs used for configuring the delay line. Each
         * parameter affects a specific aspect of the delay effect.
         */
        enum param_id : dsp_param_id_t
        {
            /** Specifies the delay time in milliseconds. */
            param_delay_ms = 0,

            /** Determines the feedback level, ranging from 0.0 to just below 1.0. */
            param_feedback = 1,

            /** Controls the wet signal mix, ranging from 0.0 to 1.0. */
            param_wet = 2,

            /** Adjusts the dry signal mix, ranging from 0.0 to 1.0. */
            param_dry = 3,
        };

        dsp_delay_line_t(const uint32_t sample_rate, const uint32_t max_delay_ms, const uint32_t max_channels) noexcept
            : _sample_rate(sample_rate), _max_channels(max_channels)
        {
            const uint32_t max_delay_samples{
                static_cast<uint32_t>(static_cast<uint64_t>(sample_rate) * max_delay_ms / 1000ull)
            };

            _max_delay_samples = chlm::max<uint32_t>(1u, max_delay_samples);

            // Allocate one interleaved buffer: [frames][channels]
            const uint32_t total{ _max_delay_samples * _max_channels };
            _buffer = new float[total];
            std::memset(_buffer, 0, total * sizeof(float));

            reset(sample_rate);
        }

        ~dsp_delay_line_t() override
        {
            delete[] _buffer;
            _buffer = nullptr;
        }

        void process(dsp_process_context_t& ctx) noexcept override
        {
            if (!_buffer) return;

            const uint32_t frames{ ctx.num_frames };
            const uint32_t channels{ chlm::min(ctx.num_channels, _max_channels) };

            float* const audio{ ctx.interleaved };

            const uint32_t max_frames{ _max_delay_samples };

            uint32_t write_index{ _write_index };
            const uint32_t delay_samples{ _delay_samples };
            const uint32_t max_delay_samples{ _max_delay_samples };

            const float feedback{ _feedback };
            const float wet{ _wet };
            const float dry{ _dry };

            for (uint32_t f{ 0 }; f < frames; ++f)
            {
                // Frame indices in circular buffer
                uint32_t read_index{ write_index + max_delay_samples - delay_samples };
                if (read_index >= max_delay_samples)
                    read_index -= max_delay_samples;

                const uint32_t buf_read{ read_index * _max_channels };
                const uint32_t buf_write{ write_index * _max_channels };
                const uint32_t audio_frame_idx{ f * channels };

                for (uint32_t ch{ 0 }; ch < channels; ++ch)
                {
                    const uint32_t in_idx{ audio_frame_idx + ch };
                    const uint32_t r_idx{ buf_read + ch };
                    const uint32_t w_idx{ buf_write + ch };

                    const float in_sample{ audio[in_idx] };
                    const float delayed_sample{ _buffer[r_idx] };

                    // Output = dry input + wet delayed
                    const float out_sample{ in_sample * dry + delayed_sample * wet };
                    audio[in_idx] = out_sample;

                    // New value stored in delay line (input + feedback * delayed)
                    _buffer[w_idx] = in_sample + (delayed_sample * feedback);
                }

                // Advance circular write index
                ++write_index;
                if (write_index >= max_delay_samples)
                    write_index = 0;
            }

            _write_index = write_index;
        }

        void set_parameter(const uint32_t id, const float value) noexcept override
        {
            switch (id)
            {
                case param_delay_ms:
                    set_delay_ms(value);
                    break;

                case param_feedback:
                    _feedback = chlm::clamp(value, 0.f, 0.999f); // keep it stable
                    break;

                case param_wet:
                    _wet = chlm::clamp(value, 0.f, 1.f);
                    break;

                case param_dry:
                    _dry = chlm::clamp(value, 0.f, 1.f);
                    break;
            }
        }

        void reset(const uint32_t sample_rate) noexcept override
        {
            _sample_rate = sample_rate;
            _write_index = 0;

            const uint32_t total{ _max_delay_samples * _max_channels };
            if (_buffer)
                std::memset(_buffer, 0, total * sizeof(float));

            // Recompute delay in samples in case SR changed
            set_delay_ms(_delay_ms);
        }

        /**
         * @brief Sets the delay time in milliseconds for the delay line.
         *
         * This method configures the delay time, which determines the duration
         * of the delay effect applied to the input signal. The provided value is
         * internally clamped to ensure it falls within valid limits, and the delay
         * is converted to an equivalent number of audio samples based on the current
         * sample rate.
         *
         * @param ms The desired delay time in milliseconds. Values less than 0 are clamped
         *           to 0, and values exceeding the maximum allowable delay are clamped
         *           to fit within the buffer size.
         */
        void set_delay_ms(float ms) noexcept
        {
            if (ms < 0.f) ms = 0.f;
            _delay_ms = ms;

            const float samples_f{ static_cast<float>(_sample_rate) * ms / 1000.f };

            uint32_t samples{ static_cast<uint32_t>(samples_f + 0.5f) }; // round to nearest

            if (samples == 0)
                samples = 1; // at least 1 sample

            if (samples >= _max_delay_samples)
                samples = _max_delay_samples - 1;

            _delay_samples = samples;
        }

        /**
         * @brief Sets the feedback level of the delay line.
         *
         * This method adjusts the amount of signal feedback for the delay unit. Higher feedback values
         * result in a more sustained repetition of the delayed signal.
         *
         * @param fb The feedback level, typically ranging from 0.0 (no feedback) to just below 1.0 (maximum feedback).
         */
        void set_feedback(const float fb) noexcept
        {
            set_parameter(param_feedback, fb);
        }

        /**
         * @brief Sets the wet signal mix for the delay line.
         *
         * This method adjusts the wet signal mix, determining the proportion of the processed
         * (delayed) signal in the output relative to the direct (dry) signal.
         *
         * @param wet The wet signal mix, typically in the range of 0.0 (no wet signal)
         *            to 1.0 (fully wet signal).
         */
        void set_wet(const float wet) noexcept
        {
            set_parameter(param_wet, wet);
        }

        /**
         * @brief Sets the dry signal mix for the delay line.
         *
         * This method adjusts the dry signal mix, determining the proportion of the unprocessed
         * (direct) signal in the output relative to the processed (wet) signal.
         *
         * @param dry The dry signal mix, typically in the range of 0.0 (no dry signal)
         *            to 1.0 (fully dry signal).
         */
        void set_dry(const float dry) noexcept
        {
            set_parameter(param_dry, dry);
        }

    private:
        uint32_t _sample_rate{ 48000 };
        uint32_t _max_channels{ 2 };

        // Circular buffer
        float* _buffer{ nullptr };
        uint32_t _max_delay_samples{ 1 };
        uint32_t _write_index{ 0 };

        // Parameters
        float _delay_ms{ 250.f };
        uint32_t _delay_samples{ 1 };

        float _feedback{ 0.35f };
        float _wet{ 0.5f };
        float _dry{ 1.f };
    };
} // namespace carrot::audio
