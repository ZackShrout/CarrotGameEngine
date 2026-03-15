//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "../../Core/CoreDefines.h"

#include <cstdint>
#include <string_view>

#include "FxChain.h"

namespace carrot::audio {
    /**
     * @brief Logical audio bus identifiers.
     *
     * Audio buses represent high-level routing destinations used to group
     * and control related sounds (e.g. music, sound effects, UI).
     *
     * Buses are mixed hierarchically and ultimately routed to the master bus.
     */
    enum class audio_bus_id : uint8_t
    {
        /** Final output bus. */
        master,

        /** Background music and long-form tracks. */
        music,

        /** Gameplay sound effects. */
        sfx,

        /** User interface sounds. */
        ui,

        /** Global reverb send/return bus. */
        reverb,

        /** Invalid or unknown bus identifier. */
        unknown,

        /** Number of valid buses. */
        count
    };

    /**
     * @brief Runtime audio bus state.
     *
     * audio_bus_t represents the per-block mixing state for a single bus.
     * It exists exclusively on the audio thread and is reset or reused
     * each render block.
     *
     * Buses apply gain, pan, mute, and solo controls to all voices routed
     * through them before being mixed into their parent bus.
     *
     * @note
     * audio_bus_t:
     *  - lives entirely on the audio thread
     *  - performs no allocation during rendering
     *  - is not accessed directly from the engine thread
     */
    struct audio_bus_t
    {
        /** Whether this bus is muted. */
        bool muted{ false };

        /** Whether this bus is soloed. */
        bool soloed{ false };

        /** Linear gain applied to all audio routed through this bus. */
        float gain{ 1.f };

        /**
         * @brief Stereo pan applied at the bus level.
         *
         * Range: -1.0 (left) to +1.0 (right).
         */
        float pan{ 0.f };

        /**
         * @brief Interleaved audio buffer for this bus.
         *
         * This buffer is owned by the audio mixer and reused each render block.
         */
        float* buffer{ nullptr };

        /**
         * @brief Post-fader send level to the reverb effect bus.
         *
         * Represents the amount of audio signal that is routed from this bus
         * to the shared reverb effect bus. It determines how much of the audio
         * processed by this bus is sent to the global reverb effect.
         *
         * Range: 0.0 (no signal sent) to 1.0 (full signal sent).
         */
        float reverb_send{ 0.f };

        /**
         * @brief Pointer to the effect processing chain for this audio bus.
         *
         * The fx_chain_t structure manages a series of digital signal processing (DSP)
         * units that are applied sequentially to the audio signal routed through this
         * bus. It allows for flexible audio effects processing, such as equalization,
         * reverb, and dynamic range compression, depending on the DSP units in the chain.
         *
         * @note
         * - The fx_chain pointer may be `nullptr` if no effects are applied to the bus.
         * - The chain is processed in the order of its units during audio rendering.
         */
        fx_chain_t* fx_chain{ nullptr };
    };

    /**
     * @brief Converts a string identifier to an audio bus ID.
     *
     * Used during asset loading and configuration parsing.
     *
     * @param bus String representation of the bus (e.g. "music", "sfx")
     * @return Corresponding audio_bus_id, or audio_bus_id::unknown on failure
     */
    inline audio_bus_id audio_bus_id_from_string(const std::string_view bus)
    {
        if (bus == "music") return audio_bus_id::music;
        if (bus == "sfx") return audio_bus_id::sfx;
        if (bus == "ui") return audio_bus_id::ui;

        return audio_bus_id::unknown;
    }
} // namespace carrot::audio
