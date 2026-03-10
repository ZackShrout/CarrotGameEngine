//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <cstdint>

namespace carrot::audio {
    /**
     * @brief Discrete envelope generator stages.
     *
     * Represents the current phase of an ADSR-style amplitude envelope.
     */
    enum class envelope_stage : uint32_t
    {
        /** Envelope is inactive and output is zero. */
        idle,

        /** Rising from zero to full amplitude. */
        attack,

        /** Falling from full amplitude to sustain level. */
        decay,

        /** Holding at sustain level until note-off. */
        sustain,

        /** Falling from current level to zero. */
        release
    };

    /**
     * @brief Parameter set for an ADSR envelope.
     *
     * All time values are specified in seconds and converted to
     * per-sample increments during envelope initialization.
     *
     * @note
     * envelope_params_t is immutable during playback and is safe
     * to construct on non-real-time threads.
     */
    struct envelope_params_t
    {
        /** Time to ramp from 0.0 to 1.0. */
        float attack_seconds{ 0.01f };

        /** Time to ramp from 1.0 to sustain_level. */
        float decay_seconds{ 0.1f };

        /** Level held during sustain stage (0.0–1.0). */
        float sustain_level{ 0.8f };

        /** Time to ramp from current level to 0.0 after note-off. */
        float release_seconds{ 0.2f };
    };

    /**
     * @brief Runtime ADSR envelope state.
     *
     * envelope_t holds all state required to advance an envelope
     * sample-by-sample on the audio thread.
     *
     * This structure:
     *  - performs no allocation
     *  - contains no branching dependent on external state
     *  - is safe for real-time use
     *
     * @note
     * envelope_t is mutated exclusively on the audio thread.
     */
    struct envelope_t
    {
        /** Current envelope stage. */
        envelope_stage stage{ envelope_stage::idle };

        /** Current envelope value (typically 0.0–1.0). */
        float value{ 0.f };

        /** Per-sample increment during attack stage. */
        float attack_inc{ 0.f };

        /** Per-sample decrement during decay stage. */
        float decay_inc{ 0.f };

        /** Per-sample decrement during release stage. */
        float release_inc{ 0.f };

        /** Cached sustain level for the sustain stage. */
        float sustain_level{ 0.f };
    };

    /**
     * @brief Triggers the start of an envelope (note-on).
     *
     * Initializes the envelope for attack and computes all
     * per-sample increments based on the supplied parameters
     * and sample rate.
     *
     * @param env Envelope state to initialize
     * @param params Envelope parameter set
     * @param sample_rate Audio sample rate in Hz
     */
    void envelope_note_on(envelope_t& env, const envelope_params_t& params, float sample_rate) noexcept;

    /**
     * @brief Triggers the release phase of an envelope (note-off).
     *
     * Transitions the envelope into the release stage and computes
     * the release decrement based on the current value.
     *
     * @param env Envelope state to modify
     * @param sample_rate Audio sample rate in Hz
     * @param release_seconds Release time in seconds
     */
    void envelope_note_off(envelope_t& env, float sample_rate, float release_seconds) noexcept;

    /**
     * @brief Advances the envelope by one sample.
     *
     * This function updates the envelope stage and value
     * and returns the current envelope amplitude.
     *
     * @param env Envelope state to advance
     * @return Current envelope value
     */
    float envelope_tick(envelope_t& env) noexcept;
} // namespace carrot::audio
