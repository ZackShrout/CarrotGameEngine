//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "AudioModule.h"

#include "Audio/Backend/AudioBackend.h"
#include "Audio/Core/AudioEngine.h"
#include "Streaming/TestStreamDecoder.h"
#include "Streaming/WavStreamDecoder.h"
#include "Utils/File/FileUtils.h"

namespace carrot::audio {
    namespace {
        audio_stream_t test_stream{};
        wav_stream_decoder_t decoder;
    }

    audio_module_t::audio_module_t(const engine_audio_config_t& config) : _config{ config } {}

    void audio_module_t::init()
    {
        if (_is_initialized)
        {
            LOG_AUDIO_WARN("Audio module already initialized");
            return;
        }

        LOG_AUDIO_INFO("Initializing Audio Module...");

        _backend = create_audio_backend();
        if (!_backend)
        {
            LOG_AUDIO_FATAL("create_audio_backend() returned nullptr");
            return;
        }

        _engine = std::make_unique<audio_engine_t>();

        if (!_backend->init(_engine.get(), _config.sample_rate, _config.block_size, _config.channels))
        {
            LOG_AUDIO_FATAL("Failed to initialize audio backend");
            return;
        }

        _clock.init(_backend->sample_rate(), _backend->block_size());
        _engine->init(&_clock, _backend->channel_count());
        _backend->start();

        // Begin audio stream test
        init_audio_stream(test_stream, 2, 48000);
        std::string_view file{ utils::file::resolve_asset_path("assets/audio/Jalen's Theme.wav") };
        decoder.open(file, &test_stream);
        decoder.start();

        audio_command_t cmd{};
        cmd.type = audio_command_type::play_stream;
        cmd.play_stream.handle = allocate_voice_handle();

        cmd.play_stream.stream = &test_stream;
        cmd.play_stream.bus = audio_bus_id::sfx;
        cmd.play_stream.gain = 1.f;
        cmd.play_stream.pan  = 0.0f;
        cmd.play_stream.looping = false;

        _engine->enqueue_command(cmd);
        // End audio stream test

        _is_initialized = true;
    }

    void audio_module_t::update([[maybe_unused]] const float delta_time) noexcept
    {
        audio_event_t evt;
        while (_engine->pop_event(evt))
        {
            switch (evt.type)
            {
                case audio_event_type::voice_finished:
                    release_voice_handle(evt.handle);
                    break;
            }
        }
    }

    void audio_module_t::shutdown()
    {
        if (!_is_initialized) return;

        LOG_AUDIO_INFO("Shutting down Audio Module...");

        decoder.stop();
        _backend->stop();
        _engine->shutdown();
        _backend->shutdown();
        _engine.reset();
        _backend.reset();

        _is_initialized = false;

        LOG_AUDIO_INFO("Audio Module shutdown complete");
    }

    voice_handle_t audio_module_t::allocate_voice_handle() noexcept
    {
        uint32_t index;

        if (!_free_slots.empty())
        {
            index = _free_slots.back();
            _free_slots.pop_back();
        }
        else
        {
            index = static_cast<uint32_t>(_voice_slots.size());
            _voice_slots.emplace_back();
        }

        voice_slot_t& slot{ _voice_slots[index] };
        slot.active = true;

        return {
            .index = index,
            .generation = slot.generation
        };
    }

    void audio_module_t::release_voice_handle(const voice_handle_t handle) noexcept
    {
        if (handle.index >= _voice_slots.size())
            return;

        voice_slot_t& slot{ _voice_slots[handle.index] };

        if (slot.generation != handle.generation)
            return; // stale handle

        slot.active = false;
        slot.generation++;
        _free_slots.push_back(handle.index);
    }
} // namespace carrot::audio
