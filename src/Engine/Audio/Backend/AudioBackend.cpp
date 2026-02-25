//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "AudioBackend.h"

#if defined(CARROT_PLATFORM_COCOA)
#include "Apple/AppleAudioBackend.h"
#elif defined(CARROT_PLATFORM_WIN32)
#include "Windows/WindowsAudioBackend.h"
#elif defined(CARROT_PLATFORM_WAYLAND) || defined(CARROT_PLATFORM_X11)
#include "Linux/LinuxAudioBackend.h"
#endif

#include "Core/Logger.h"
#include "Null/NullAudioBackend.h"

namespace carrot::audio {
    std::unique_ptr<audio_backend_t> create_audio_backend() noexcept
    {
#if defined(CARROT_PLATFORM_COCOA)
        return std::make_unique<apple_audio_backend_t>();
#elif defined(CARROT_PLATFORM_WIN32)
        return std::make_unique<windows_audio_backend_t>();
#elif defined(CARROT_PLATFORM_WAYLAND) || defined(CARROT_PLATFORM_X11)
        return std::make_unique<linux_audio_backend_t>();
#else
        LOG_AUDIO_WARN("Platform audio not implemented yet, returning null backend");
        return std::make_unique<null_audio_backend_t>();
#endif
    }
} // namespace carrot::audio