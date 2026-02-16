//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Audio/DSP/Envelope.h"
#include "Audio/Mixer/AudioBus.h"
#include "Audio/Sample/AudioSample.h"
#include "Audio/Core/AudioCore.h"

#include <chlm/Core.h>

#include "VoiceHandle.h"

namespace carrot::audio {
    enum class voice_state : uint8_t
    {
        idle,
        active,
        releasing,
    };

    enum class voice_type : uint8_t
    {
        sample,
    };

    /**
     * @brief Single active audio voice.
     *
     * Lives entirely on the audio thread.
     */
    struct voice_t
    {
        voice_state state{ voice_state::idle };
        voice_type type{ voice_type::sample };
        audio_bus_id bus{ audio_bus_id::sfx };
        spatial_mode spatial{ spatial_mode::none };

        uint32_t generation{ 0 };
        voice_handle_t handle{ };

        float pan{ 0.f }; // -1 = left, 0 = center, +1 = right
        float gain{ 0.2f };

        chlm::float3 position{ 0.f };

        float max_distance{ 50.f }; // world units
        float ref_distance{ 1.f }; // near-field

        const audio_sample_t* sample{ nullptr };
        uint32_t sample_cursor{ };

        bool looping{ false };
        uint32_t loop_start{ 0 };
        uint32_t loop_end{ 0 }; // 0 = end of sample

        bool paused{ false };

        uint64_t start_frame{ 0 };
        envelope_t envelope;
    };

    inline float voice_next_sample(voice_t& voice, [[maybe_unused]] const double sample_rate) noexcept
    {
        switch (voice.type)
        {
            case voice_type::sample:
            {
                if (voice.paused)
                    return 0.0f; // silence, no cursor advance

                const uint32_t sample_end{
                    voice.looping && voice.loop_end > 0 ? voice.loop_end : voice.sample->frame_count
                };

                if (voice.sample_cursor >= sample_end)
                {
                    if (voice.looping)
                    {
                        voice.sample_cursor = voice.loop_start;
                    }
                    else
                    {
                        voice.state = voice_state::releasing;
                        return 0.0f;
                    }
                }

                const uint32_t idx{ voice.sample_cursor++ * voice.sample->channels };

                return voice.sample->data[idx];
            }
        }

        return 0.0f;
    }
} // namespace carrot::audio
