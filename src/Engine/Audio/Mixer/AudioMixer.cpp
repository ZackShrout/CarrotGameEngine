//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "AudioMixer.h"

#include <cstring>

#include "Audio/DSP/Pan.h"

namespace carrot::audio {
    void audio_mixer_t::init(const uint32_t max_frames, const uint32_t channels) noexcept
    {
        _channels = channels;
        _max_frames = max_frames;
        const uint32_t total{ max_frames * channels };

        for (auto& bus: _buses)
        {
            bus.buffer = new float[total]; // allocated ONCE at init
            bus.gain = 1.0f;
        }
    }

    void audio_mixer_t::shutdown() noexcept
    {
        for (auto& bus: _buses)
        {
            delete[] bus.buffer;
            bus.buffer = nullptr;
        }
    }

    void audio_mixer_t::clear(const uint32_t frame_count) const noexcept
    {
        const uint32_t total{ frame_count * _channels };

        for (auto& bus: _buses)
            std::memset(bus.buffer, 0, total * sizeof(float));
    }

    float* audio_mixer_t::bus_buffer(audio_bus_id id) const noexcept
    {
        return _buses[static_cast<size_t>(id)].buffer;
    }

    float* audio_mixer_t::master_buffer() const noexcept
    {
        return bus_buffer(audio_bus_id::master);
    }

    void audio_mixer_t::mix_bus_into_master(audio_bus_id id, const uint32_t frame_count) const noexcept
    {
        if (id == audio_bus_id::master)
            return;

        const audio_bus_t& src{ _buses[static_cast<size_t>(id)] };
        const audio_bus_t& dst{ _buses[static_cast<size_t>(audio_bus_id::master)] };

        if (src.muted) return;

        if (!src.soloed && any_bus_soloed()) return;

        const uint32_t total{ frame_count * _channels };
        const float gain{ src.gain };

        float pan_l{ 1.f };
        float pan_r{ 1.f };
        compute_pan_gains(src.pan, pan_l, pan_r);

        for (uint32_t frame{ 0 }; frame < frame_count; ++frame)
        {
            const uint32_t i{ frame * _channels };

            dst.buffer[i + 0] += src.buffer[i + 0] * gain * pan_l;
            dst.buffer[i + 1] += src.buffer[i + 1] * gain * pan_r;
        }
    }

    // PRIVATE

    bool audio_mixer_t::any_bus_soloed() const noexcept
    {
        for (size_t i{ 0 }; i < _buses.size(); ++i)
        {
            if (static_cast<audio_bus_id>(i) == audio_bus_id::master)
                continue;

            if (_buses[i].soloed)
                return true;
        }

        return false;
    }
} // namespace carrot::audio
