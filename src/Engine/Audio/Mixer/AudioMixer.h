//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "AudioBus.h"

#include <array>

namespace carrot::audio {
    class audio_mixer_t
    {
    public:
        void init(uint32_t max_frames, uint32_t channels) noexcept;
        void shutdown() noexcept;

        void clear(uint32_t frame_count) const noexcept;

        [[nodiscard]] float* bus_buffer(audio_bus_id id) const noexcept;
        [[nodiscard]] float* master_buffer() const noexcept;

        void mix_bus_into_master(audio_bus_id id, uint32_t frame_count) const noexcept;

        void set_bus_gain(audio_bus_id id, const float gain) noexcept { _buses[static_cast<size_t>(id)].gain = gain; }
        void set_bus_mute(audio_bus_id id, const bool muted) noexcept { _buses[static_cast<size_t>(id)].muted = muted; }
        void set_bus_solo(audio_bus_id id, const bool soloed) noexcept { _buses[static_cast<size_t>(id)].soloed = soloed; }
        void set_bus_pan(audio_bus_id id, const float pan) noexcept { _buses[static_cast<size_t>(id)].pan = pan; }

    private:
        [[nodiscard]] bool any_bus_soloed() const noexcept;

        std::array<audio_bus_t, static_cast<size_t>(audio_bus_id::count)> _buses{ };

        uint32_t _channels{ 0 };
        uint32_t _max_frames{ 0 };
    };
} // namespace carrot::audio
