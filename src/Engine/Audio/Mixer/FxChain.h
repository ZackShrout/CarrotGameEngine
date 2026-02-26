//
// Created by Zack Shrout on 2/25/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Audio/DSP/DspUnit.h"

#include <array>

namespace carrot::audio {
    constexpr size_t k_max_fx_per_bus{ 8 };

    class fx_chain_t
    {
    public:
        fx_chain_t() = default;

        // Add a DSP unit pointer to the chain (at init time, NOT real-time)
        bool add(dsp_unit_t* unit, const bool bypass = false) noexcept
        {
            if (_count >= k_max_fx_per_bus)
                return false;

            _units[_count] = unit;
            _bypass[_count] = bypass;
            ++_count;

            return true;
        }

        // Remove all units (RT-safe if done outside process())
        void clear() noexcept { _count = 0; }

        // Quick access to change bypass at runtime via parameters
        void set_bypass(const size_t index, const bool bypass) noexcept
        {
            if (index < _count)
                _bypass[index] = bypass;
        }

        void set_bypass_all(const bool bypass) noexcept
        {
            for (size_t i{ 0 }; i < _count; ++i)
                _bypass[i] = bypass;
        }

        void process(dsp_process_context_t& ctx) const noexcept
        {
            for (size_t i{ 0 }; i < _count; ++i)
            {
                if (!_bypass[i] && _units[i])
                    _units[i]->process(ctx);
            }
        }

        void reset(const uint32_t sample_rate) const noexcept
        {
            for (size_t i{ 0 }; i < _count; ++i)
            {
                if (_units[i])
                    _units[i]->reset(sample_rate);
            }
        }

        [[nodiscard]] bool empty() const noexcept { return _count == 0; }
        [[nodiscard]] size_t size() const noexcept { return _count; }
    private:
        std::array<dsp_unit_t*, k_max_fx_per_bus> _units{ };
        std::array<bool, k_max_fx_per_bus> _bypass{ false };
        size_t _count{ 0 };
    };
} // namespace carrot::audio