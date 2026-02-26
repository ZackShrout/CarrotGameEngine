//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "AudioBus.h"
#include "Audio/Core/AudioCore.h"

#include <array>

#include "Audio/DSP/BiquadFilter.h"
#include "Audio/DSP/SchroederReverb.h"

namespace carrot::audio {
    /**
     * @brief Real-time audio mixer.
     *
     * audio_mixer_t is responsible for accumulating per-bus audio buffers,
     * applying bus-level controls (gain, pan, mute, solo), and mixing all
     * buses into the master output buffer.
     *
     * The mixer exists entirely on the audio thread and is invoked from
     * audio_engine_t::render().
     *
     * @note
     * audio_mixer_t:
     *  - performs no dynamic allocation during rendering
     *  - owns all per-bus mixing buffers
     *  - must be initialized before use
     *  - is not thread-safe and must never be accessed from outside the audio thread
     */
    class audio_mixer_t
    {
    public:
        /**
         * @brief Initializes the audio mixer.
         *
         * Allocates internal bus buffers sized for the maximum render block.
         *
         * @param max_frames Maximum number of frames per render call
         * @param channels Number of output channels
         */
        void init(uint32_t max_frames, uint32_t channels) noexcept;

        /**
         * @brief Shuts down the audio mixer and releases all internal buffers.
         *
         * Must be called when the audio thread is no longer running.
         */
        void shutdown() noexcept;

        /**
         * @brief Clears all bus buffers for a render block.
         *
         * @param frame_count Number of frames to clear
         */
        void clear(uint32_t frame_count) const noexcept;

        /**
         * @brief Returns the buffer for a specific audio bus.
         *
         * Voices write their output directly into the buffer
         * corresponding to their routed bus.
         *
         * @param id Audio bus identifier
         * @return Pointer to the interleaved bus buffer
         */
        [[nodiscard]] float* bus_buffer(audio_bus_id id) const noexcept;

        /**
         * @brief Returns the master output buffer.
         *
         * @return Pointer to the interleaved master buffer
         */
        [[nodiscard]] float* master_buffer() const noexcept;

        /**
         * @brief Mixes a bus into the master output buffer.
         *
         * Applies bus-level mute, solo, gain, and pan controls before
         * accumulating the bus signal into the master buffer.
         *
         * @param id Audio bus to mix
         * @param frame_count Number of frames to mix
         */
        void mix_bus_into_master(audio_bus_id id, uint32_t frame_count) const noexcept;

        /**
         * @brief Retrieves the effects chain associated with a specific audio bus.
         *
         * This method provides access to the effects chain for a given audio bus,
         * allowing modifications or processing of the effects applied to that bus.
         * The effects chain is stored internally by the mixer and is identified by
         * the provided audio bus ID.
         *
         * @param id The identifier of the audio bus whose effects chain is to be retrieved.
         * @return A reference to the effects chain (fx_chain_t) associated with the specified audio bus.
         *
         * @note
         * - The behavior is undefined if the provided audio bus ID is invalid.
         * - This method does not allocate any resources and assumes the audio bus IDs
         *   are valid and initialized.
         */
        fx_chain_t& bus_fx_chain(audio_bus_id id) noexcept { return _bus_fx[static_cast<size_t>(id)]; }

        /**
         * @brief Processes the effects chain for all active audio buses.
         *
         * Iterates over all audio buses, applying the associated effects chain
         * to each bus's audio buffer in the context of the given frame count
         * and sample rate.
         *
         * @param frame_count Number of frames in the current render block.
         * @param sample_rate Sample rate of the audio processing context.
         */
        void process_bus_fx(uint32_t frame_count, uint32_t sample_rate) const noexcept;

        /**
         * @brief Accumulates audio data from all buses into the reverb send buffer.
         *
         * This method processes all audio buses, applies their reverb send gain,
         * and accumulates the results into the dedicated reverb send buffer.
         * The reverb buffer is cleared at the beginning of the process to ensure
         * the new data is mixed cleanly. Buses with a reverb send gain of zero
         * or those reserved for master or reverb outputs are skipped.
         *
         * @param frame_count The number of audio frames to process.
         *                    Determines the size of the buffer being processed.
         */
        void accumulate_reverb_send(uint32_t frame_count) const noexcept;

        /**
         * @brief Sets the linear gain for a bus.
         *
         * @param id Audio bus identifier
         * @param gain Linear gain multiplier
         */
        void set_bus_gain(audio_bus_id id, const float gain) noexcept { _buses[static_cast<size_t>(id)].gain = gain; }

        /**
         * @brief Mutes or unmutes a bus.
         *
         * @param id Audio bus identifier
         * @param muted True to mute the bus
         */
        void set_bus_mute(audio_bus_id id, const bool muted) noexcept { _buses[static_cast<size_t>(id)].muted = muted; }

        /**
         * @brief Sets whether a bus is soloed.
         *
         * When any bus is soloed, only soloed buses contribute to the
         * master output.
         *
         * @param id Audio bus identifier
         * @param soloed True to solo the bus
         */
        void set_bus_solo(audio_bus_id id, const bool soloed) noexcept
        {
            _buses[static_cast<size_t>(id)].soloed = soloed;
        }

        /**
         * @brief Sets the stereo pan for a bus.
         *
         * @param id Audio bus identifier
         * @param pan Stereo pan (-1.0 = left, +1.0 = right)
         */
        void set_bus_pan(audio_bus_id id, const float pan) noexcept { _buses[static_cast<size_t>(id)].pan = pan; }

        /**
         * @brief Sets the reverb send level for a specific audio bus.
         *
         * Adjusts the amount of signal routed from the specified audio bus
         * to the global reverb bus, allowing control over the contribution
         * of the bus to the reverb effects.
         *
         * @param id Audio bus identifier
         * @param send Reverb send level (0.0 = no send, 1.0 = full send)
         */
        void set_bus_reverb_send(audio_bus_id id, const float send) noexcept
        {
            _buses[static_cast<size_t>(id)].reverb_send = send;
        }

    private:
        /**
         * @brief Checks whether any bus is currently soloed.
         *
         * @return True if at least one bus is soloed
         */
        [[nodiscard]] bool any_bus_soloed() const noexcept;

        /**
         * @brief Configures the reverb bus for the audio mixer.
         *
         * This method sets up the processing chain for the reverb bus by configuring
         * high-pass, low-pass, and reverb effect parameters. It ensures the proper
         * frequency, quality factor (Q), gain, and other reverb-specific parameters are
         * applied. The configured effects are added to the processing chain, which is
         * responsible for handling reverb audio processing within the mixer.
         *
         * @note
         * configure_reverb_bus():
         *  - Initializes and configures high-pass and low-pass filters for the reverb bus.
         *  - Configures room size, dampening, pre-delay, wet/dry mix, and stereo width for the reverb processor.
         *  - Updates the effect chain for the reverb audio bus, clearing previous effects and
         *    adding the configured effects to the chain.
         */
        void configure_reverb_bus();

        /** Per-bus mixing state and buffers. */
        std::array<audio_bus_t, static_cast<size_t>(audio_bus_id::count)> _buses{ };

        /**
         * @brief Array of effects chains for each audio bus.
         *
         * `_bus_fx` stores the processing chains associated with all audio buses,
         * enabling the application of audio effects (e.g., reverb, echo, distortion)
         * to each bus individually before mixing.
         *
         * Each element in the array corresponds to a specific audio bus, indexed
         * by `audio_bus_id`. The size of the array matches the total number of
         * available audio buses.
         *
         * @note
         * - `_bus_fx` must be properly initialized with valid `fx_chain_t` instances
         *   before use.
         * - Modifications to the effects chains must be thread-safe if interacting
         *   with the audio rendering process.
         */
        std::array<fx_chain_t, static_cast<size_t>(audio_bus_id::count)> _bus_fx{};

        /** Number of output channels. */
        uint32_t _channels{ 0 };

        /** Maximum frames supported per render block. */
        uint32_t _max_frames{ 0 };

        // Built-in reverb bus FX
        /** High-pass filter for the reverb bus. */
        dsp_biquad_filter_t _reverb_bus_hp{ biquad_type::highpass, k_engine_sample_rate };

        /** Low-pass filter for the reverb bus. */
        dsp_biquad_filter_t _reverb_bus_lp{ biquad_type::lowpass, k_engine_sample_rate };

        /** Per-bus Schroeder reverb processor. */
        dsp_schroeder_reverb_t _reverb_bus_verb{ k_engine_sample_rate };
    };
} // namespace carrot::audio
