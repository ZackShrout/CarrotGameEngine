//
// Created by Zack Shrout on 2/25/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>

namespace carrot::audio {
    struct dsp_process_context_t
    {
        float* interleaved;   // [num_frames * num_channels]
        uint32_t num_channels;
        uint32_t num_frames;
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

        // In-place processing, called once per render block.
        virtual void process(dsp_process_context_t& ctx) noexcept = 0;

        // Generic parameter interface (id is unit-defined).
        virtual void set_parameter(uint32_t id, float value) noexcept = 0;

        // Reset internal state (e.g. when sample rate changes).
        virtual void reset(uint32_t sample_rate) noexcept = 0;
    };
} // namespace carrot::audio