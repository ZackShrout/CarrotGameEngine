//
// Created by Zack Shrout on 2/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "Audio.h"

#include "Assets/AssetManager.h"
#include "Assets/AssetService.h"
#include "AudioModule.h"

namespace carrot::audio {
    namespace {
        float apply_variance(const float value, const float variance)
        {
            if (variance <= 0.0f)
                return value;

            const float r{ 1.0f/* + chlm::rand_range(-variance, variance)*/ };

            return value * r;
        }
    } // anonymous namespace

    voice_handle_t play(const assets::loaded_audio_asset_t& asset) noexcept
    {
        const sound_play_params_t params{ };
        return play(asset, params);
    }

    voice_handle_t play(const assets::loaded_audio_asset_t& asset, const sound_play_params_t& params) noexcept
    {
        if (!asset.valid() || !asset.record) return voice_handle_t::invalid();

        const assets::audio_asset_record_t& record{ *asset.record };

        audio_module_t& audio{ audio_service_t::get() };
        voice_handle_t handle{ audio.allocate_voice_handle() };

        audio_command_t cmd{ };

        const float gain{ apply_variance(record.gain * params.gain, record.gain_variance) };
        const float pitch{ apply_variance(record.pitch * params.pitch, record.pitch_variance) };
        const spatial_mode spatial{ params.override_spatial ? params.spatial_override : record.spatial };
        const float pan{ params.pan != 0.f ? params.pan : record.pan };

        if (!record.streamed)
        {
            if (!asset.sample)
            {
                LOG_AUDIO_ERROR("Loaded non-stream audio asset {}  has no sample", record.logical_id);
                return voice_handle_t::invalid();
            }

            // ── Sample-based voice ──────────────────
            cmd.type = audio_command_type::play_sound;
            cmd.play_sound.handle = handle;

            cmd.play_sound.sample = asset.sample.get();
            cmd.play_sound.bus    = record.bus;

            cmd.play_sound.gain   = gain;
            cmd.play_sound.pitch  = pitch;

            cmd.play_sound.spatial      = spatial;
            cmd.play_sound.pan          = pan;
            cmd.play_sound.distance     = record.distance;
            cmd.play_sound.min_distance = record.min_distance;
            cmd.play_sound.max_distance = record.max_distance;
            cmd.play_sound.position     = params.position;

            cmd.play_sound.looping    = record.looping;
            cmd.play_sound.loop_start = record.loop_start;
            cmd.play_sound.loop_end   = record.loop_end;
        }
        else
        {
            // ── Streaming voice ─────────────────────

            audio_stream_t* stream = audio.create_stream_from_asset(asset);
            if (!stream)
            {
                LOG_AUDIO_ERROR("Failed to create stream for asset '{}'", record.logical_id);
                return voice_handle_t::invalid();
            }

            stream->owning_voice = handle;

            cmd.type = audio_command_type::play_stream;
            cmd.play_stream.handle = handle;

            cmd.play_stream.stream = stream;
            cmd.play_stream.bus    = record.bus;

            cmd.play_stream.gain = gain;
            cmd.play_stream.pan  = pan;

            cmd.play_stream.looping = record.looping;
            cmd.play_stream.loop_start = record.loop_start;
            cmd.play_stream.loop_end = record.loop_end;
        }

        // ── Enqueue ──────────────────────────────
        audio.engine().enqueue_command(cmd);

        return handle;
    }

    voice_handle_t play(std::string_view asset_name)
    {
        const assets::loaded_audio_asset_t* asset{ assets::asset_service_t::manager().audio().get(asset_name) };

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
