//
// Created by Zack Shrout on 2/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "AudioService.h"

#include "Audio/AudioModule.h"

namespace carrot::audio {
    void audio_service_t::provide(audio_module_t* instance) noexcept
    {
        // We still overwrite — last write wins (helps hot-reload experiments)
        if (_instance != nullptr) [[unlikely]]
                LOG_AUDIO_WARN("Audio service already provided — double registration?");

        _instance = instance;
    }

    audio_module_t& audio_service_t::get()
    {
        if (_instance == nullptr) [[unlikely]]
                LOG_AUDIO_FATAL("Attempted to use audio_service_t::get() before audio_module_t was provided");

        return *_instance;
    }
} // namespace carrot::audio
