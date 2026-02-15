//
// Created by Zack Shrout on 2/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Audio.h"
#include "AudioModule.h"
#include "Assets/Audio/AudioAssetRegistry.h"
#include "Assets/AssetService.h"

namespace carrot::audio {
    namespace {
        float apply_variance(float value, float variance)
        {
            if (variance <= 0.0f)
                return value;

            const float r{ 1.0f/* + chlm::rand_range(-variance, variance)*/ };

            return value * r;
        }
    } // anonymous namespace

    void play(const assets::audio_asset_t& asset) noexcept
    {
        sound_play_params_t params{ };
        play(asset, params);
    }

    void play(const assets::audio_asset_t& asset, const sound_play_params_t& params) noexcept
    {
        audio_command_t cmd{ };
        cmd.type = audio_command_type::play_sound;

        // ── Sample / Routing ─────────────────────
        cmd.play_sound.sample = asset.sample;
        cmd.play_sound.bus = asset.bus;

        // ── Gain / Pitch ─────────────────────────
        cmd.play_sound.gain = apply_variance(asset.gain * params.gain, asset.gain_variance);
        cmd.play_sound.pitch = apply_variance(asset.pitch * params.pitch, asset.pitch_variance);

        // ── Spatial ──────────────────────────────
        cmd.play_sound.spatial = params.override_spatial ? params.spatial_override : asset.spatial;
        cmd.play_sound.pan = params.pan != 0.0f ? params.pan : asset.pan;
        cmd.play_sound.distance = asset.distance;
        cmd.play_sound.min_distance = asset.min_distance;
        cmd.play_sound.max_distance = asset.max_distance;
        cmd.play_sound.position = params.position;

        // ── Enqueue ──────────────────────────────
        audio_service_t::get().engine().enqueue_command(cmd);
    }

    void play(std::string_view asset_name)
    {
        const assets::audio_asset_registry_t& registry{ assets::asset_service_t::audio() };
        const assets::asset_id_t id{ assets::make_asset_id(asset_name) };

        const auto handle{ registry.find(id) };
        if (!handle)
        {
            LOG_AUDIO_ERROR("Unknown audio asset '{}'", asset_name);
            return;
        }

        const assets::audio_asset_t* asset{ registry.get(handle) };
        if (!asset)
        {
            LOG_AUDIO_ERROR("Failed to resolve asset '{}'", asset_name);
            return;
        }

        play(*asset);
    }
} // namespace carrot::audio
