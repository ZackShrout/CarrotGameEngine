//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>
#include <type_traits>

#include "Audio/Mixer/AudioBus.h"
#include "Audio/Sample/AudioSample.h"

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
        play_sine,
        play_sample,
        stop_all,

        set_bus_gain,
        set_bus_mute,
        set_bus_solo,

        set_bus_pan,
        set_voice_pan,

        set_voice_gain,
    };

    /**
     * @brief Payload for a play_sine command.
     */
    struct audio_cmd_play_sine
    {
        float frequency;
        float gain;
        audio_bus_id bus;
    };

    struct play_sample_cmd
    {
        const audio_sample_t* sample;
        float gain;
        audio_bus_id bus;
    };

    struct set_bus_gain_cmd
    {
        audio_bus_id bus;
        float gain;
    };

    struct set_bus_flag_cmd
    {
        audio_bus_id bus;
        bool enabled;
    };

    struct set_bus_pan_cmd
    {
        audio_bus_id bus;
        float pan;
    };

    struct set_voice_pan_cmd
    {
        uint32_t voice_index;
        float pan;
    };

    struct set_voice_gain_cmd
    {
        uint32_t voice_index;
        float gain;
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
            audio_cmd_play_sine play_sine;
            play_sample_cmd play_sample;
            set_bus_gain_cmd set_bus_gain;
            set_bus_flag_cmd set_bus_mute;
            set_bus_flag_cmd set_bus_solo;
            set_bus_pan_cmd set_bus_pan;
            set_voice_pan_cmd set_voice_pan;
            set_voice_gain_cmd set_voice_gain;
        };
    };

    static_assert(std::is_trivially_copyable_v<audio_command_t>);
} // namespace carrot::audio
