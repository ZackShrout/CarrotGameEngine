//
// Created by Zack Shrout on 2/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/AssetHandle.h"
#include "Audio/AudioTypes.h"
#include "Audio/Sample/AudioSample.h"
#include "Audio/Mixer/AudioBus.h"

namespace carrot::assets {
    /**
     * @brief Declarative description of how an audio asset behaves when played.
     *
     * Audio assets are immutable, shareable, and contain no playback state.
     * Each call to play() spawns a new voice instance using this description.
     */
    struct audio_asset_t
    {
        /** @brief PCM audio sample used for playback. */
        const audio::audio_sample_t* sample{ nullptr };

        /** @brief Output bus the sound is routed to. */
        audio::audio_bus_id bus{ audio::audio_bus_id::sfx };

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
        audio::spatial_mode spatial{ audio::spatial_mode::none };

        /**
         * @brief Default stereo pan for non-3D sounds.
         *
         * Range: -1.0 (left) to +1.0 (right).
         * Ignored for full 3D spatialization.
         */
        float pan{ 0.0f };

        /** @brief Distance attenuation model used when spatialized. */
        audio::distance_model distance{ audio::distance_model::none };

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

        /** @brief Whether this asset loops. */
        bool looping{ false };

        /**
         * @brief Loop region start (in sample frames).
         *
         * If looping is true and loop_end > loop_start,
         * playback jumps here when reaching loop_end.
         */
        uint32_t loop_start{ 0 };

        /**
         * @brief Loop region end (in sample frames).
         *
         * A value of 0 indicates "end of sample".
         */
        uint32_t loop_end{ 0 };

        /** @brief True if this asset should be streamed from disk. */
        bool streamed{ false };
    };
} // namespace carrot::assets
