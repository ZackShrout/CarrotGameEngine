//
// Created by Zack Shrout on 2/25/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>

namespace carrot::audio {
    using dsp_param_id_t = uint32_t;

    /**
     * @struct dsp_process_context_t
     * @brief Context for digital signal processing (DSP) operations.
     *
     * @details The `dsp_process_context_t` structure encapsulates all necessary
     *          information required for performing in-place audio processing.
     *          It provides access to the interleaved audio sample buffer,
     *          along with metadata about the number of audio channels and frames.
     *
     * @note The sample rate is provided to DSP units via dsp_unit_t::reset()
     *       and is assumed constant between reset calls.
     */
    struct dsp_process_context_t
    {
        /**
         * @brief Pointer to the interleaved audio sample buffer.
         *
         * The buffer contains [num_frames * num_channels] samples, in-place.
         */
        float* interleaved;

        /** Number of audio channels in the buffer. */
        uint32_t num_channels;

        /** Number of audio frames in this processing block. */
        uint32_t num_frames;

        /**
         * @var sample_rate
         * @brief Sample rate of the audio stream in Hertz (Hz).
         *
         * @details This variable represents the number of audio samples per second
         *          used during digital signal processing (DSP). It defines the temporal
         *          resolution of the audio stream, dictating the fidelity and bandwidth
         *          of the audio content being processed.
         *
         * @details The sample rate is critical for ensuring that DSP routines, such as
         *          filters, effects, and time-domain processes, operate correctly and
         *          maintain synchronization with the intended playback or recording rate.
         *          Common sample rates include 44,100 Hz for CD-quality audio and 48,000 Hz
         *          for professional audio production.
         *
         * @note The value of this variable must remain consistent across processing stages
         *       to avoid timing mismatches, distortions, or artifacts in the audio output.
         */
        uint32_t sample_rate;
    };

    /**
     * @class dsp_unit_t
     * @brief Abstract interface for a digital signal processing (DSP) unit.
     *
     * This class provides a common base for implementing DSP units, which are
     * responsible for processing audio data in-place, managing parameters, and
     * handling state resets. It is designed to operate entirely on the audio
     * processing thread.
     *
     * @note Used for bus/voice FX.
     */
    class dsp_unit_t
    {
    public:
        virtual ~dsp_unit_t() = default;

        /**
         * @brief Processes audio data in-place for a single rendering block.
         *
         * This method is called once per rendering block to perform in-place processing
         * of audio data using the provided context. Implementations of this method
         * should ensure thread-safety and avoid operations that are not real-time safe.
         *
         * @param ctx A reference to a dsp_process_context_t structure containing audio
         * data and metadata. The audio data is stored interleaved for the specified
         * number of frames and channels, along with additional processing parameters
         * such as the sample rate.
         */
        virtual void process(dsp_process_context_t& ctx) noexcept = 0;

        /**
         * @brief Sets the value of a parameter identified by a specific ID.
         *
         * This method allows dynamic adjustment of DSP unit parameters during runtime.
         * The ID uniquely identifies a parameter within the DSP unit, and the value
         * specifies the new desired setting for that parameter. Implementations should
         * ensure that parameter updates are thread-safe and do not compromise real-time
         * processing requirements.
         *
         * @param id The unique identifier of the parameter to update.
         * @param value The new value to set for the parameter.
         */
        virtual void set_parameter(uint32_t id, float value) noexcept = 0;

        /**
         * @brief Resets the internal state of the DSP unit.
         *
         * This method is called to reinitialize the internal state of the DSP unit, such as when the
         * sample rate changes or when a full state reset is required. Implementations should ensure
         * proper handling of state reset to maintain consistent behavior in subsequent processing.
         *
         * @param sample_rate The new sample rate, in Hz, to be used by the DSP unit.
         */
        virtual void reset(uint32_t sample_rate) noexcept = 0;
    };
} // namespace carrot::audio
