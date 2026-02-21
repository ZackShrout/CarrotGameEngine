//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Audio/AudioTypes.h"

#include <cstdint>

namespace carrot::audio {
        constexpr uint32_t k_max_voices{ 64 };
        constexpr uint32_t k_buffer_frames{ 48000 * 4 }; // 4 seconds @ 48kHz
        constexpr uint32_t k_max_channels{ 2 };
        constexpr uint32_t k_stream_chunk_frames{ 1024 }; // possibly 512?
} // namespace core::audio
