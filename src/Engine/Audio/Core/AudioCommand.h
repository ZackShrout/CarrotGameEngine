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
     * @brief Audio command types sent from the engine thread to the audio thread.
     *
     * Audio commands are used to request changes to the real-time audio state
     * from non-real-time code. Commands are enqueued on the engine thread and
     * consumed by the audio thread during rendering.
     *
     * All commands must be:
     *  - trivially copyable
     *  - fixed-size
     *  - allocation-free
     *  - safe to enqueue from non-real-time threads
     */
    enum class audio_command_type : uint8_t
    {
        /** Create and begin playback of a new voice instance. */
        play_sound,

        /** Stop all currently active voices. */
        stop_all,

        /** Pause a specific voice instance. */
        pause_voice,

        /** Resume a previously paused voice instance. */
        resume_voice,

        /** Stop a specific voice instance using envelope release. */
        stop_voice,

        /** Set the gain of an audio bus. */
        set_bus_gain,

        /** Enable or disable muting of an audio bus. */
        set_bus_mute,

        /** Enable or disable soloing of an audio bus. */
        set_bus_solo,

        /** Set stereo pan for an audio bus. */
        set_bus_pan,

        /** Set stereo pan for a specific voice instance. */
        set_voice_pan,

        /** Set gain for a specific voice instance. */
        set_voice_gain,

        /** Set spatialization mode for a specific voice instance. */
        set_voice_spatial,

        /** Set world-space position for a spatialized voice instance. */
        set_voice_position,
    };

    /**
     * @brief Command data for creating and starting a new voice instance.
     */
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

        bool looping;
        uint32_t loop_start;
        uint32_t loop_end;
    };

    /**
     * @brief Command data for pausing a voice instance.
     */
    struct pause_voice_cmd
    {
        voice_handle_t handle;
    };

    /**
     * @brief Command data for resuming a paused voice instance.
     */
    struct resume_voice_cmd
    {
        voice_handle_t handle;
    };

    /**
     * @brief Command data for stopping a voice instance.
     *
     * Stopping a voice initiates envelope release and eventual destruction.
     */
    struct stop_voice_cmd
    {
        voice_handle_t handle;
    };

    /**
     * @brief Command data for setting the gain of an audio bus.
     */
    struct set_bus_gain_cmd
    {
        audio_bus_id bus{ };
        float gain;
    };

    /**
     * @brief Command data for enabling or disabling a bus flag.
     *
     * Used for mute and solo operations.
     */
    struct set_bus_flag_cmd
    {
        audio_bus_id bus{ };
        bool enabled;
    };

    /**
     * @brief Command data for setting stereo pan on an audio bus.
     */
    struct set_bus_pan_cmd
    {
        audio_bus_id bus{ };
        float pan;
    };

    /**
     * @brief Command data for setting stereo pan on a voice instance.
     */
    struct set_voice_pan_cmd
    {
        voice_handle_t handle{ };
        float pan;
    };

    /**
     * @brief Command data for setting gain on a voice instance.
     */
    struct set_voice_gain_cmd
    {
        voice_handle_t handle{ };
        float gain;
    };

    /**
     * @brief Command data for setting spatialization mode on a voice instance.
     */
    struct set_voice_spatial_cmd
    {
        voice_handle_t handle{ };
        spatial_mode mode;
    };

    /**
     * @brief Command data for setting the world-space position of a voice instance.
     */
    struct set_voice_position_cmd
    {
        voice_handle_t handle{ };
        float x;
        float y;
        float z;
    };

    /**
     * @brief Single audio command.
     *
     * audio_command_t is a tagged union containing the data for exactly one
     * audio command. The active union member is selected by the @ref type field.
     *
     * This structure is designed to be passed through a lock-free queue and
     * consumed by the audio thread without allocation or dynamic dispatch.
     */
    struct audio_command_t
    {
        /** Type tag identifying the active command. */
        audio_command_type type;

        /** Command payload. */
        union
        {
            play_sound_cmd play_sound;
            pause_voice_cmd pause_voice;
            resume_voice_cmd resume_voice;
            stop_voice_cmd stop_voice;
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

    /**
     * @brief Ensures audio commands are safe for lock-free queue transport.
     */
    static_assert(std::is_trivially_copyable_v<audio_command_t>);
} // namespace carrot::audio
