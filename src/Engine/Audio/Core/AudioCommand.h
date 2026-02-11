//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>
#include <type_traits>

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
        play_sine, // test command
        stop_all,
    };

    /**
     * @brief Payload for a play_sine command.
     */
    struct audio_cmd_play_sine
    {
        float frequency;
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
        };
    };

    static_assert(std::is_trivially_copyable_v<audio_command_t>);
} // namespace carrot::audio
