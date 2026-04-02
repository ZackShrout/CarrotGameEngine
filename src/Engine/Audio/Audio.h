//
// Created by Zack Shrout on 2/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Assets/Audio/AudioAsset.h"
#include "AudioTypes.h"
#include "Core/AudioService.h"
#include "Voice/VoiceHandle.h"

namespace carrot::audio {
    /**
     * @brief Plays a sound using its authored default configuration.
     *
     * Creates a new voice instance and begins playback immediately.
     * Playback parameters such as gain, pitch, routing, looping, and spatial
     * settings are taken directly from the audio asset.
     *
     * This function is safe to call from the engine thread. The actual audio
     * playback is performed asynchronously on the audio thread.
     *
     * @param asset Loaded audio asset to play
     * @return Handle identifying the newly created voice instance.
     *         The handle may be used to pause, resume, or stop playback.
     */
    voice_handle_t play(const assets::loaded_audio_asset_t& asset) noexcept;

    /**
     * @brief Plays a sound with explicit playback override parameters.
     *
     * Creates a new voice instance and begins playback immediately.
     * The supplied parameters override the authored defaults on the audio asset
     * for this voice instance only.
     *
     * This function is safe to call from the engine thread. The actual audio
     * playback is performed asynchronously on the audio thread.
     *
     * @param asset  Loaded audio asset to play
     * @param params Playback parameters overriding the asset defaults
     * @return Handle identifying the newly created voice instance.
     *         The handle may be used to pause, resume, or stop playback.
     */
    voice_handle_t play(const assets::loaded_audio_asset_t& asset, const sound_play_params_t& params) noexcept;

    /**
     * @brief Plays a loaded audio asset by stable string identifier.
     *
     * Resolves the asset name through the audio asset registry and plays the
     * asset using its authored default configuration.
     *
     * If the asset cannot be resolved, an invalid voice handle is returned and
     * no sound is played.
     *
     * @param asset_name Stable asset identifier (e.g. "sfx.beep", "music.world").
     * @return Handle identifying the newly created voice instance, or an invalid
     *         handle if the asset could not be resolved.
     */
    voice_handle_t play(std::string_view asset_name);

    /**
     * @brief Pauses playback of a voice instance.
     *
     * Pausing a voice freezes its playback position without destroying the
     * underlying voice. Loop state, sample cursor, and envelope state are
     * preserved and playback may be resumed later.
     *
     * Calling pause() on an invalid or stale handle has no effect.
     *
     * @param handle Handle identifying the voice instance to pause
     */
    void pause(voice_handle_t handle) noexcept;

    /**
     * @brief Resumes playback of a previously paused voice instance.
     *
     * Playback continues from the exact position at which the voice was paused.
     * If the voice is not currently paused, this function has no effect.
     *
     * Calling resume() on an invalid or stale handle has no effect.
     *
     * @param handle Handle identifying the voice instance to resume
     */
    void resume(voice_handle_t handle) noexcept;

    /**
     * @brief Stops playback of a voice instance.
     *
     * Stopping a voice initiates an envelope release, allowing the sound to
     * fade out smoothly before the voice is destroyed. Once the release phase
     * completes, the voice handle is reclaimed and becomes invalid.
     *
     * Calling stop() on an invalid or stale handle has no effect.
     *
     * @param handle Handle identifying the voice instance to stop
     */
    void stop(voice_handle_t handle) noexcept;
} // namespace carrot::audio
