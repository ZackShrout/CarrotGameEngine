//
// Created by Zack Shrout on 2/25/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Audio/DSP/DspUnit.h"

#include <array>

namespace carrot::audio {
    constexpr size_t k_max_fx_per_bus{ 4 }; // enough for [HP/LP]→[Reverb]→[EQ]→[Limiter] etc.

    struct fx_chain_t
    {
        std::array<dsp_unit_t*, k_max_fx_per_bus> units{ };
        size_t count{ 0 };

        void process(dsp_process_context_t& ctx) const noexcept
        {
            for (size_t i{ 0 }; i < count; ++i)
                units[i]->process(ctx);
        }

        void reset(const uint32_t sample_rate) const noexcept
        {
            for (size_t i{ 0 }; i < count; ++i)
                units[i]->reset(sample_rate);
        }
    };
} // namespace carrot::audio