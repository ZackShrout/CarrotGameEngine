//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "AudioModule.h"

#include "Audio/Backend/AudioBackend.h"
#include "Audio/Core/AudioEngine.h"

namespace carrot::audio {
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

        _is_initialized = true;
    }

    void audio_module_t::shutdown()
    {
        if (!_is_initialized) return;

        LOG_AUDIO_INFO("Shutting down Audio Module...");

        _backend->stop();
        _engine->shutdown();
        _backend->shutdown();
        _engine.reset();
        _backend.reset();

        _is_initialized = false;

        LOG_AUDIO_INFO("Audio Module shutdown complete");
    }
} // namespace carrot::audio
