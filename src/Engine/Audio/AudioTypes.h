//
// Created by Zack Shrout on 2/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>
#include <chlm/CarrotHLM.h>

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

    /**
     * @brief Per-playback override parameters for sound playback.
     *
     * These parameters allow callers to override selected aspects
     * of a sound asset without modifying the asset itself.
     *
     * Any value left at default will fall back to the sound asset's
     * configuration.
     */
    struct sound_play_params_t
    {
        /** @brief Gain multiplier applied on top of asset gain. */
        float gain{ 1.0f };

        /** @brief Pitch multiplier applied on top of asset pitch. */
        float pitch{ 1.0f };

        /**
         * @brief Stereo pan override.
         *
         * Range: -1.0 (left) to +1.0 (right).
         */
        float pan{ 0.0f };

        /**
         * @brief Optional spatial mode override.
         *
         * Only applied if override_spatial is true.
         */
        spatial_mode spatial_override{ spatial_mode::none };

        /** @brief Enables spatial mode override. */
        bool override_spatial{ false };

        /**
         * @brief World-space position of the sound.
         *
         * Used for planar and 3D spatialization modes.
         */
        chlm::float3 position{ 0.0f, 0.0f, 0.0f };
    };
} // namespace carrot::audio
