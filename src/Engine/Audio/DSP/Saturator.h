//
// Created by Zack Shrout on 2/26/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "DspUnit.h"

#include <chlm/CarrotHLM.h>
#include <cmath>
#include <algorithm>

namespace carrot::audio {
    /**
     * @class dsp_saturator_t
     * @brief A digital signal processing (DSP) unit for applying audio saturation effects.
     *
     * The `dsp_saturator_t` class implements a non-linear audio processing unit that
     * introduces harmonic distortion and saturation to the input audio signal. It allows
     * customization through several adjustable parameters, including drive intensity,
     * wet/dry mix ratio, output gain, and shaping curvature. The saturation effect is
     * achieved by applying a transfer function that soft-clips the input signal, resulting
     * in a warmer and more harmonically rich output.
     *
     * This class is designed to be lightweight, efficient, and suitable for real-time
     * audio processing in digital audio workstations (DAWs), audio plugins, or other audio
     * applications. It inherits from the `dsp_unit_t` base class and overrides key methods
     * for audio processing and parameter management.
     */
    class dsp_saturator_t final : public dsp_unit_t
    {
    public:
        /**
         * @enum param_id
         * @brief Enumeration of parameter IDs for the dsp_saturator_t class.
         *
         * This enumeration defines the identifiers for parameters used in the
         * dsp_saturator_t class to control the behavior of the saturation effect.
         * Each ID corresponds to a specific parameter that can be modified to
         * customize the audio processing.
         */
        enum param_id : uint32_t
        {
            /** Determines the intensity of the drive applied to the shaping function. */
            param_drive = 0,

            /** Controls the wet/dry mix of the saturation effect (0 for dry, 1 for wet). */
            param_mix = 1,

            /** Specifies the linear gain applied to the output after saturation. */
            param_output_gain = 2
        };

        dsp_saturator_t() noexcept = default;

        void process(dsp_process_context_t& ctx) noexcept override
        {
            float* const buffer{ ctx.interleaved };

            for (uint32_t i{ 0 }; i < ctx.num_frames * ctx.num_channels; ++i)
            {
                const float in{ buffer[i] };

                // Apply drive
                const float driven{ in * _drive };

                // Soft clip via simple rational function: x / (1 + k|x|)
                const float ax{ std::fabs(driven) };
                const float shaped{ driven / (1.f + _shape_k * ax) };

                // Mix dry/wet
                float out{ in * (1.f - _mix) + shaped * _mix };

                // Output gain
                out *= _output_gain;

                buffer[i] = out;
            }
        }

        void set_parameter(const uint32_t id, const float value) noexcept override
        {
            switch (id)
            {
                case param_drive:
                    _drive = std::max(0.f, value);
                    break;
                case param_mix:
                    _mix = chlm::clamp(value, 0.f, 1.f);
                    break;
                case param_output_gain:
                    _output_gain = value;
                    break;
            }
        }

        void reset(uint32_t /*sample_rate*/) noexcept override { /* no-op */ }

        /**
         * @brief Sets the drive intensity for the saturation effect.
         *
         * Adjusts the strength of the input signal before it is processed
         * by the saturation effect's shaping function. Higher values of the
         * drive parameter result in increased harmonic distortion and
         * saturation, while lower values produce a more subtle effect.
         *
         * @param d The drive intensity as a floating-point value.
         *          Typically ranges from 0.0 (no drive) to higher values
         *          for more saturation.
         */
        void set_drive(const float d) noexcept { set_parameter(param_drive, d); }

        /**
         * @brief Sets the wet/dry mix level of the saturation effect.
         *
         * Adjusts the balance between the unprocessed (dry) and processed (wet)
         * signals included in the output. A value of 0.0 corresponds to a completely
         * dry signal, while 1.0 produces only the processed (wet) signal. Intermediate
         * values blend the two signals proportionally, enabling precise control over
         * the effect's presence in the result.
         *
         * @param m The mix level where 0.0 represents fully dry and 1.0 represents
         *          fully wet.
         */
        void set_mix(const float m) noexcept { set_parameter(param_mix, m); }

        /**
         * @brief Sets the output gain for the saturation effect.
         *
         * This method adjusts the output gain parameter, which controls the amplitude
         * of the processed audio signal after the saturation effect has been applied.
         * Modifying this value allows fine-tuning of the overall output level to achieve
         * the desired balance in the audio mix.
         *
         * @param g The desired output gain value. Typically, this is a normalized
         *          floating-point value that determines the amplification or attenuation
         *          applied to the output signal. Higher values increase the output level,
         *          while lower values reduce it.
         */
        void set_output_gain(const float g) noexcept { set_parameter(param_output_gain, g); }

        /**
         * @brief Sets the shaping curvature parameter for the saturation effect.
         *
         * This method adjusts the shaping curvature parameter, which influences
         * the non-linear transfer function applied to the audio signal. A higher
         * value results in more pronounced harmonic distortion. The parameter is
         * clamped to a minimum value of 0.0 to ensure stable behavior.
         *
         * @param k The desired shaping curvature value. Must be greater than or equal to 0.0.
         */
        void set_shape_k(const float k) noexcept { _shape_k = std::max(0.f, k); }

    private:
        /**
         * @brief The multiplier that determines the intensity of the saturation effect.
         *
         * This variable adjusts the strength of the drive applied to the input signal
         * before it is processed by the shaping function. Higher values increase the
         * harmonic saturation and distortion, while a value of 1.0 represents unity gain
         * with no additional drive applied. This parameter is typically used to create
         * more aggressive or subtle saturation effects based on the user's requirements.
         */
        float _drive{ 1.f };

        /**
         * @brief The wet/dry mix level of the saturation effect.
         *
         * This variable controls the balance between the unprocessed (dry) and processed (wet)
         * signals in the output. A value of 0.0 results in a completely dry signal, while a
         * value of 1.0 outputs only the processed (wet) signal. Intermediate values blend the
         * two signals proportionally, allowing for fine-tuned control over the effect's intensity.
         * By default, the value is set to fully wet (1.0).
         */
        float _mix{ 1.f };

        /**
         * @brief The linear gain applied to the output signal after saturation.
         *
         * This variable determines the final amplitude of the processed audio signal
         * by scaling the output. It plays a critical role in adjusting the overall
         * loudness of the signal after the saturation effect has been applied. A value
         * of 1.0 represents unity gain, meaning the output amplitude remains unchanged,
         * while higher or lower values can be used to amplify or attenuate the signal.
         * This parameter is typically used to ensure consistent output levels or to
         * achieve desired loudness based on the configuration of other saturation parameters.
         */
        float _output_gain{ 1.0f };

        /**
         * @brief Curvature constant used in the shaping function for saturation.
         *
         * The `_shape_k` variable defines the intensity of the non-linearity applied
         * by the saturation effect's shaping function. It adjusts the curvature of
         * the transfer function, influencing the harmonic content of the processed
         * audio signal. Higher values of `_shape_k` result in stronger bending,
         * increasing the saturation effect and harmonic distortion, while lower
         * values produce a more linear and subtle effect. The default value is set
         * to 0.5.
         */
        float _shape_k{ 0.5f };
    };
} // namespace carrot::audio
