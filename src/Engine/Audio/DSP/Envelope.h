//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>

namespace carrot::audio {
    enum class envelope_stage : uint8_t
    {
        idle,
        attack,
        decay,
        sustain,
        release
    };

    struct envelope_params_t
    {
        float attack_seconds{ 0.01f };
        float decay_seconds{ 0.1f };
        float sustain_level{ 0.8f };
        float release_seconds{ 0.2f };
    };

    struct envelope_t
    {
        envelope_stage stage{ envelope_stage::idle };
        float value{ 0.0f };

        float attack_inc{ 0.0f };
        float decay_inc{ 0.0f };
        float release_inc{ 0.0f };

        float sustain_level{ 0.0f };
    };

    void envelope_note_on(envelope_t& env, const envelope_params_t& params, float sample_rate) noexcept;
    void envelope_note_off(envelope_t& env, float sample_rate, float release_seconds) noexcept;
    float envelope_tick(envelope_t& env) noexcept;
} // namespace carrot::audio
