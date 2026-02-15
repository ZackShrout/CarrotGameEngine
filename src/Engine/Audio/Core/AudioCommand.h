//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>
#include <type_traits>

#include "Audio/Mixer/AudioBus.h"
#include "Audio/Sample/AudioSample.h"
#include "Audio/Voice/Voice.h"
#include "Audio/Voice/VoiceHandle.h"

namespace carrot::audio {
    /**
     * @brief Audio command types sent from game thread to audio thread.
     *
     * Commands must be:
     *  - trivially copyable
     *  - fixed size
     *  - allocation-free
     */
    enum class audio_command_type : uint8_t
    {
        play_sound,
        stop_all,

        set_bus_gain,
        set_bus_mute,
        set_bus_solo,

        set_bus_pan,
        set_voice_pan,

        set_voice_gain,
        set_voice_spatial,
        set_voice_position,
    };

    struct play_sound_cmd
    {
        const audio_sample_t* sample;

        audio_bus_id bus;
        voice_handle_t handle;

        spatial_mode spatial;

        float gain;
        float pitch;
        float pan;

        distance_model distance;
        float min_distance;
        float max_distance;

        chlm::float3 position;
    };

    struct set_bus_gain_cmd
    {
        audio_bus_id bus{ };
        float gain;
    };

    struct set_bus_flag_cmd
    {
        audio_bus_id bus{ };
        bool enabled;
    };

    struct set_bus_pan_cmd
    {
        audio_bus_id bus{ };
        float pan;
    };

    struct set_voice_pan_cmd
    {
        uint32_t voice_index;
        voice_handle_t handle{ };
        float pan;
    };

    struct set_voice_gain_cmd
    {
        uint32_t voice_index;
        voice_handle_t handle{ };
        float gain;
    };

    struct set_voice_spatial_cmd
    {
        uint32_t voice_index;
        voice_handle_t handle{ };
        spatial_mode mode;
    };

    struct set_voice_position_cmd
    {
        uint32_t voice_index;
        voice_handle_t handle{ };
        float x;
        float y;
        float z;
    };

    /**
     * @brief Single audio command.
     *
     * This is a tagged union to keep the queue simple and fast.
     */
    struct audio_command_t
    {
        audio_command_type type;

        union
        {
            play_sound_cmd play_sound;
            set_bus_gain_cmd set_bus_gain;
            set_bus_flag_cmd set_bus_mute;
            set_bus_flag_cmd set_bus_solo;
            set_bus_pan_cmd set_bus_pan;
            set_voice_pan_cmd set_voice_pan;
            set_voice_gain_cmd set_voice_gain;
            set_voice_spatial_cmd set_voice_spatial;
            set_voice_position_cmd set_voice_position;
        };
    };

    static_assert(std::is_trivially_copyable_v<audio_command_t>);
} // namespace carrot::audio
