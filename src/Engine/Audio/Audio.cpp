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

    voice_handle_t play(const assets::audio_asset_t& asset) noexcept
    {
        const sound_play_params_t params{ };
        return play(asset, params);
    }

    voice_handle_t play(const assets::audio_asset_t& asset, const sound_play_params_t& params) noexcept
    {
        audio_module_t& audio{ audio_service_t::get() };
        voice_handle_t handle{ audio.allocate_voice_handle() };

        audio_command_t cmd{ };

        const float gain{ apply_variance(asset.gain * params.gain, asset.gain_variance) };
        const float pitch{ apply_variance(asset.pitch * params.pitch, asset.pitch_variance) };
        const spatial_mode spatial{ params.override_spatial ? params.spatial_override : asset.spatial };
        const float pan{ params.pan != 0.0f ? params.pan : asset.pan };

        if (!asset.streamed)
        {
            // ── Sample-based voice ──────────────────
            cmd.type = audio_command_type::play_sound;
            cmd.play_sound.handle = handle;

            cmd.play_sound.sample = asset.sample;
            cmd.play_sound.bus    = asset.bus;

            cmd.play_sound.gain   = gain;
            cmd.play_sound.pitch  = pitch;

            cmd.play_sound.spatial      = spatial;
            cmd.play_sound.pan          = pan;
            cmd.play_sound.distance     = asset.distance;
            cmd.play_sound.min_distance = asset.min_distance;
            cmd.play_sound.max_distance = asset.max_distance;
            cmd.play_sound.position     = params.position;

            cmd.play_sound.looping    = asset.looping;
            cmd.play_sound.loop_start = asset.loop_start;
            cmd.play_sound.loop_end   = asset.loop_end;
        }
        else
        {
            // ── Streaming voice ─────────────────────

            audio_stream_t* stream = audio.create_stream_from_asset(asset);
            if (!stream)
            {
                LOG_AUDIO_ERROR("Failed to create stream for asset '{}'",
                                asset.file_path.string());
                return voice_handle_t::invalid();
            }

            cmd.type = audio_command_type::play_stream;
            cmd.play_stream.handle = handle;

            cmd.play_stream.stream = stream;
            cmd.play_stream.bus    = asset.bus;

            cmd.play_stream.gain = gain;
            cmd.play_stream.pan  = pan;

            // For v1, streaming assets are treated as:
            // - non-spatial (spatial_mode::none)
            // - and let voice.stereo logic handle them
            cmd.play_stream.looping = asset.looping;
        }

        // ── Enqueue ──────────────────────────────
        audio.engine().enqueue_command(cmd);

        return handle;
    }

    voice_handle_t play(std::string_view asset_name)
    {
        const assets::audio_asset_registry_t& registry{ assets::asset_service_t::audio() };
        const assets::asset_id_t id{ assets::make_asset_id(asset_name) };

        const auto handle{ registry.find(id) };
        if (!handle)
        {
            LOG_AUDIO_ERROR("Unknown audio asset '{}'", asset_name);
            return voice_handle_t::invalid();
        }

        const assets::audio_asset_t* asset{ registry.get(handle) };
        if (!asset)
        {
            LOG_AUDIO_ERROR("Failed to resolve asset '{}'", asset_name);
            return voice_handle_t::invalid();
        }

        return play(*asset);
    }

    void pause(voice_handle_t handle) noexcept
    {
        audio_module_t& audio{ audio_service_t::get() };
        audio_command_t cmd{};
        cmd.type = audio_command_type::pause_voice;
        cmd.pause_voice.handle = handle;

        audio.engine().enqueue_command(cmd);
    }

    void resume(voice_handle_t handle) noexcept
    {
        audio_module_t& audio{ audio_service_t::get() };
        audio_command_t cmd{};
        cmd.type = audio_command_type::resume_voice;
        cmd.pause_voice.handle = handle;

        audio.engine().enqueue_command(cmd);
    }

    void stop(voice_handle_t handle) noexcept
    {
        audio_module_t& audio{ audio_service_t::get() };
        audio_command_t cmd{};
        cmd.type = audio_command_type::stop_voice;
        cmd.stop_voice.handle = handle;

        audio.engine().enqueue_command(cmd);
    }
} // namespace carrot::audio
