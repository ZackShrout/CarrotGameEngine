//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Envelope.h"

namespace carrot::audio {
    void envelope_note_on(envelope_t& env, const envelope_params_t& params, const float sample_rate) noexcept
    {
        env.stage = envelope_stage::attack;
        env.value = 0.f;

        env.attack_inc = params.attack_seconds > 0.f ? 1.f / (params.attack_seconds * sample_rate) : 1.f;

        env.decay_inc = params.decay_seconds > 0.f
                            ? (1.f - params.sustain_level) / (params.decay_seconds * sample_rate)
                            : 1.f;

        env.release_inc = params.release_seconds > 0.f
                              ? params.sustain_level / (params.release_seconds * sample_rate)
                              : 1.f;

        env.sustain_level = params.sustain_level;
    }

    void envelope_note_off(envelope_t& env, const float sample_rate, const float release_seconds) noexcept
    {
        env.stage = envelope_stage::release;
        env.release_inc = release_seconds > 0.f ? env.value / (release_seconds * sample_rate) : env.value;
    }

    float envelope_tick(envelope_t& env) noexcept
    {
        switch (env.stage)
        {
            case envelope_stage::idle:
                return 0.f;

            case envelope_stage::attack:
                env.value += env.attack_inc;
                if (env.value >= 1.0f)
                {
                    env.value = 1.0f;
                    env.stage = envelope_stage::decay;
                }
                break;

            case envelope_stage::decay:
                env.value -= env.decay_inc;
                if (env.value <= env.sustain_level)
                {
                    env.value = env.sustain_level;
                    env.stage = envelope_stage::sustain;
                }
                break;

            case envelope_stage::sustain:
                break;

            case envelope_stage::release:
                env.value -= env.release_inc;
                if (env.value <= 0.0f)
                {
                    env.value = 0.0f;
                    env.stage = envelope_stage::idle;
                }
                break;
        }

        return env.value;
    }
} // namespace carrot::audio
