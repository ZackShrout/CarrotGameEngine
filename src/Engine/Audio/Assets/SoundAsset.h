//
// Created by Zack Shrout on 2/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Audio/AudioTypes.h"
#include "Audio/Sample/AudioSample.h"
#include "Audio/Mixer/AudioBus.h"

namespace carrot::audio {
/**
 * @brief Declarative description of how a sound should behave when played.
 *
 * A sound asset represents a reusable sound definition that combines:
 *  - sample data
 *  - routing information
 *  - spatial behavior
 *  - default gain and pitch
 *
 * Sound assets are immutable at runtime and may be safely referenced
 * by multiple voices concurrently.
 *
 * @note
 * A sound asset is NOT a voice and does not represent playback state.
 * Each call to play() creates a new voice instance.
 */
struct sound_asset_t
{
    /** @brief PCM audio sample used for playback. */
    const audio_sample_t* sample{ nullptr };

    /** @brief Output bus the sound is routed to. */
    audio_bus_id bus{ audio_bus_id::sfx };

    /** @brief Base gain applied to the sound (linear). */
    float gain{ 1.0f };

    /** @brief Base pitch multiplier (1.0 = original pitch). */
    float pitch{ 1.0f };

    /**
     * @brief Random gain variation applied per playback.
     *
     * A value of 0.1 allows ±10% gain variation.
     */
    float gain_variance{ 0.0f };

    /**
     * @brief Random pitch variation applied per playback.
     *
     * Useful for reducing repetition in frequently played sounds.
     */
    float pitch_variance{ 0.0f };

    /** @brief Spatialization mode for this sound. */
    spatial_mode spatial{ spatial_mode::none };

    /**
     * @brief Default stereo pan for non-3D sounds.
     *
     * Range: -1.0 (left) to +1.0 (right).
     * Ignored for full 3D spatialization.
     */
    float pan{ 0.0f };

    /** @brief Distance attenuation model used when spatialized. */
    distance_model distance{ distance_model::none };

    /** @brief Distance at which the sound is played at full volume. */
    float min_distance{ 1.0f };

    /** @brief Distance beyond which the sound is fully attenuated. */
    float max_distance{ 50.0f };

    /**
     * @brief Maximum number of concurrent voices for this sound.
     *
     * A value of 0 indicates no explicit limit.
     */
    uint8_t max_voices{ 0 };

    /**
     * @brief Voice priority used when stealing is required.
     *
     * Higher values indicate higher priority.
     */
    uint8_t priority{ 128 };
};

} // namespace carrot::audio
