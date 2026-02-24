//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "AudioModule.h"

#include "Assets/Audio/AudioAsset.h"
#include "Audio/Backend/AudioBackend.h"
#include "Audio/Core/AudioEngine.h"
#include "Streaming/WavStreamDecoder.h"
#include "Utils/File/FileUtils.h"

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

        LOG_AUDIO_INFO("Backend sample rate: {}, config sample rate: {}",
               _backend->sample_rate(),
               _config.sample_rate);

        _clock.init(k_engine_sample_rate, _backend->block_size());
        _engine->init(&_clock, _backend->channel_count(), _backend->sample_rate());

        LOG_AUDIO_INFO("Clock sample rate: {}, engine channels: {}",
               _clock.sample_rate(),
               _backend->channel_count());

        _backend->start();

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
                {
                    for (const auto& stream : _streams)
                    {
                        if (stream->owning_voice == evt.handle)
                            free_stream(stream.get());
                    }

                    release_voice_handle(evt.handle);
                    break;
                }
            }
        }
    }

    void audio_module_t::shutdown()
    {
        if (!_is_initialized) return;

        LOG_AUDIO_INFO("Shutting down Audio Module...");

        // Stop all active streams
        for (const auto& stream : _streams)
        {
            if (stream->active)
                stream->decoder.stop();
        }

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

    audio_stream_t* audio_module_t::allocate_stream() noexcept
    {
        if (!_free_stream_indices.empty())
        {
            const uint32_t idx{ _free_stream_indices.back() };
            _free_stream_indices.pop_back();

            std::unique_ptr<audio_stream_t>& slot = _streams[idx];

            if (!slot)
                slot = std::make_unique<audio_stream_t>();

            audio_stream_t* stream = slot.get();
            stream->active = true;

            return stream;
        }

        _streams.emplace_back(std::make_unique<audio_stream_t>());
        audio_stream_t* stream{ _streams.back().get() };
        stream->active = true;

        return stream;
    }

    void audio_module_t::free_stream(audio_stream_t* stream)
    {
        if (!stream)
            return;

        // Stop its decoder, mark inactive
        stream->decoder.stop();
        stream->active = false;

        // Find its index in _streams (v1: linear search is fine)
        for (uint32_t i{ 0 }; i < _streams.size(); ++i)
        {
            if (_streams[i].get() == stream)
            {
                _free_stream_indices.push_back(i);
                return;
            }
        }

        // If we don't find it, something is wrong; you might want an assert here
        CE_ASSERT(false, "Attempted to free unknown audio_stream_t*");
    }

    audio_stream_t* audio_module_t::create_stream_from_asset(const assets::audio_asset_t& asset) noexcept
    {
        audio_stream_t* stream{ allocate_stream() };
        if (!stream)
        {
            LOG_AUDIO_ERROR("Failed to allocate audio stream for '{}'", asset.file_path.string());
            return nullptr;
        }

        // For now, assume stereo
        constexpr uint32_t channels{ 2 }; // TODO: probe file if you want
        constexpr uint32_t sample_rate{ k_engine_sample_rate };

        init_audio_stream(*stream, channels, sample_rate);
        stream->looping = asset.looping;
        stream->loop_start = asset.loop_start;
        stream->loop_end = asset.loop_end;

        if (!stream->decoder.open(asset.file_path.string(), stream))
            return nullptr;

        stream->decoder.start();

        return stream;
    }
} // namespace carrot::audio
