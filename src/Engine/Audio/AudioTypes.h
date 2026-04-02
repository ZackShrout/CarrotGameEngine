//
// Created by Zack Shrout on 2/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <chlm/CarrotHLM.h>
#include <cstdint>

namespace carrot::audio {
    /**
     * @brief Defines how a sound is spatialized relative to the listener.
     *
     * Spatial mode controls whether and how a sound's world position
     * influences panning and distance attenuation.
     *
     * This allows the same sound asset to be used across:
     *  - non-positional 2D playback (UI, music)
     *  - planar worlds (top-down, side-scroller)
     *  - full 3D environments
     */
    enum class spatial_mode : uint8_t
    {
        /**
         * @brief No spatial processing.
         *
         * The sound is treated as a purely 2D source.
         * Position, distance attenuation, and spatial panning are ignored.
         */
        none,

        /**
         * @brief Planar (2D world) spatialization.
         *
         * The sound is positioned on a horizontal plane relative to the listener.
         * Distance attenuation and stereo panning are applied.
         *
         * Ideal for side-scrollers, top-down games, and HD-2D worlds.
         */
        planar,

        /**
         * @brief Full 3D spatialization.
         *
         * The sound exists in full 3D space relative to the listener.
         * Future implementations may include HRTF, cones, occlusion,
         * and environmental effects.
         */
        full_3d,

        /**
         * @brief Invalid or unrecognized spatial mode.
         *
         * Used when parsing fails or input data is malformed.
         */
        unknown
    };

    /**
     * @brief Distance attenuation model for spatialized sounds.
     *
     * Determines how volume decreases as a sound moves away
     * from the listener.
     */
    enum class distance_model : uint8_t
    {
        /** @brief No distance attenuation is applied. */
        none,

        /** @brief Linear attenuation between reference and maximum distance. */
        linear,

        /**
         * @brief Inverse distance attenuation.
         *
         * Produces a physically inspired falloff curve.
         */
        inverse,

        /**
         * @brief Exponential attenuation.
         *
         * Produces a steeper falloff than inverse distance.
         */
        exponential,

        /**
         * @brief Invalid or unrecognized distance model.
         */
        unknown
    };

    /**
     * @brief Per-playback override parameters for sound playback.
     *
     * These parameters allow callers to override selected aspects
     * of a sound asset at play time without modifying the asset itself.
     *
     * Any value left at its default will fall back to the
     * sound asset's authored configuration.
     */
    struct sound_play_params_t
    {
        /** @brief Gain multiplier applied on top of asset gain. */
        float gain{ 1.f };

        /** @brief Pitch multiplier applied on top of asset pitch. */
        float pitch{ 1.f };

        /**
         * @brief Stereo pan override.
         *
         * Range:
         *  - -1.0 = full left
         *  -  0.0 = center
         *  - +1.0 = full right
         */
        float pan{ 0.f };

        /**
         * @brief Optional spatial mode override.
         *
         * Only applied if @ref override_spatial is true.
         */
        spatial_mode spatial_override{ spatial_mode::none };

        /**
         * @brief Enables spatial mode override.
         *
         * When false, the sound asset's spatial mode is used.
         */
        bool override_spatial{ false };

        /**
         * @brief World-space position of the sound.
         *
         * Used when spatialization is enabled (planar or 3D).
         */
        chlm::float3 position{ 0.f, 0.f, 0.f };
    };

    /**
     * @brief Converts a string to a spatial_mode enum.
     *
     * @param mode String representation (e.g. "none", "planar", "full_3d")
     * @return Corresponding spatial_mode, or spatial_mode::unknown on failure
     */
    inline spatial_mode spatial_mode_from_string(const std::string_view mode)
    {
        if (mode == "none") return spatial_mode::none;
        if (mode == "planar") return spatial_mode::planar;
        if (mode == "full_3d") return spatial_mode::full_3d;

        return spatial_mode::unknown;
    }

    /**
     * @brief Converts a string to a distance_model enum.
     *
     * @param model String representation (e.g. "linear", "inverse")
     * @return Corresponding distance_model, or distance_model::unknown on failure
     */
    inline distance_model distance_model_from_string(const std::string_view model)
    {
        if (model == "none") return distance_model::none;
        if (model == "linear") return distance_model::linear;
        if (model == "inverse") return distance_model::inverse;
        if (model == "exponential") return distance_model::exponential;

        return distance_model::unknown;
    }
} // namespace carrot::audio
