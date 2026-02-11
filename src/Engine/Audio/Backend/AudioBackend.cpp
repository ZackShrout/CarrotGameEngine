//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "AudioBackend.h"

#include "Null/NullAudioBackend.h"

namespace carrot::audio {
    std::unique_ptr<audio_backend_t> create_audio_backend()
    {
        return std::make_unique<null_audio_backend_t>();
    }
} // namespace carrot::audio