//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

namespace carrot::audio {
    /**
     * @brief Single active audio voice.
     *
     * Lives entirely on the audio thread.
     */
    struct voice_t
    {
        bool   active{ false };

        double phase{ 0.0 };
        double frequency{ 440.0 };
        float  gain{ 0.2f };
    };
} // namespace carrot::audio