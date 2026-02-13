//
// Created by Zack Shrout on 2/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

namespace carrot::audio {
    /**
     * @brief Defines how a sound is spatialized in the world.
     *
     * Spatial mode determines how the engine interprets a sound's position,
     * panning, and distance attenuation.
     *
     * This allows the same sound asset to be used in:
     *  - pure 2D contexts (UI, music)
     *  - 2.5D / planar worlds
     *  - full 3D scenes
     */
    enum class spatial_mode : uint8_t
    {
        /**
         * @brief No spatial processing.
         *
         * The sound is treated as a non-positional 2D sound.
         * Distance attenuation and positional panning are ignored.
         */
        none,

        /**
         * @brief Planar (2D world) spatialization.
         *
         * The sound is positioned along a horizontal plane relative
         * to the listener and affects stereo panning and attenuation.
         *
         * Ideal for side-scrollers, top-down games, and HD-2D worlds.
         */
        planar,

        /**
         * @brief Full 3D spatialization.
         *
         * The sound exists in full 3D space relative to the listener.
         * Future implementations may include HRTF, cones, and occlusion.
         */
        full_3d
    };

    /**
     * @brief Distance attenuation model used for spatialized sounds.
     *
     * Determines how volume falls off as a sound moves away
     * from the listener.
     */
    enum class distance_model : uint8_t
    {
        /** @brief No distance attenuation is applied. */
        none,

        /** @brief Linear attenuation between min and max distance. */
        linear,

        /** @brief Inverse distance attenuation (physically inspired). */
        inverse,

        /** @brief Exponential attenuation for steeper falloff. */
        exponential
    };

    enum class voice_state : uint8_t
    {
        idle,
        active,
        releasing,
    };

    enum class voice_type : uint8_t
    {
        sine,
        sample,
    };

    enum class audio_bus_id : uint8_t
    {
        master,
        music,
        sfx,
        ui,

        count
    };
} // namespace carrot::audio
