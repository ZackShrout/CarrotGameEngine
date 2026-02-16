//
// Created by Zack Shrout on 2/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "AudioTypes.h"
#include "Assets/Audio/AudioAsset.h"
#include "Core/AudioService.h"
#include "Voice/VoiceHandle.h"

namespace carrot::audio {
    /**
     * @brief Plays a sound using its default configuration.
     *
     * Creates a new voice instance and begins playback immediately.
     *
     * @param asset Sound asset to play
     * @return Handle to the instance of the asset playing
     */
    voice_handle_t play(const assets::audio_asset_t& asset) noexcept;

    /**
     * @brief Plays a sound with explicit playback parameters.
     *
     * @param asset Sound asset to play
     * @param params Playback override parameters
     * @return Handle to the instance of the asset playing
     */
    voice_handle_t play(const assets::audio_asset_t& asset, const sound_play_params_t& params) noexcept;

    /**
     * @brief Play an audio asset by string ID using its authored defaults.
     *
     * @param asset_name Stable asset identifier (e.g. "sfx.beep").
     * @return Handle to the instance of the asset playing
     */
    voice_handle_t play(std::string_view asset_name);

    void pause(voice_handle_t handle) noexcept;


    void resume(voice_handle_t handle) noexcept;
} // namespace carrot::audio
