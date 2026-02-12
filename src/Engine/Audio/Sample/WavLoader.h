//
// Created by Zack Shrout on 2/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "AudioSample.h"

#include <string_view>

namespace carrot::audio {
    /**
         * @brief Load a WAV file fully into memory as float PCM.
         *
         * @note Must NOT be called from the audio thread.
         * @note Returned sample is immutable and audio-thread safe.
         *
         * @param path Path to .wav file
         * @return Loaded audio sample, or nullptr on failure
         */
    audio_sample_t* load_wav_file(std::string_view path);
} // namespace carrot::audio
