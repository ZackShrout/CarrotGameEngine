//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#pragma once

#include "Audio/Backend/AudioBackend.h"

#include <AudioUnit/AudioUnit.h>

namespace carrot::audio {

    /**
     * @brief macOS Core Audio backend using HAL Output AudioUnit.
     *
     * Provides low-latency, pull-based audio via Core Audio without
     * Objective-C or AVAudioEngine.
     */
    class apple_audio_backend_t final : public audio_backend_t
    {
    public:
        apple_audio_backend_t() = default;
        ~apple_audio_backend_t() override = default;

        bool init(
            audio_callback_t* callback,
            uint32_t sample_rate,
            uint32_t block_size,
            uint32_t channels) noexcept override;

        void start() noexcept override;
        void stop() noexcept override;
        void shutdown() noexcept override;

        [[nodiscard]] uint32_t sample_rate() const noexcept override { return _sample_rate; }
        [[nodiscard]] uint32_t block_size() const noexcept override { return _block_size; }
        [[nodiscard]] uint32_t channel_count() const noexcept override { return _channels; }

    private:
        static OSStatus render_callback(
            void*                       in_ref_con,
            AudioUnitRenderActionFlags* io_action_flags,
            const AudioTimeStamp*       in_time_stamp,
            UInt32                      in_bus_number,
            UInt32                      in_number_frames,
            AudioBufferList*            io_data);

        audio_callback_t* _callback{ nullptr };

        AudioUnit _audio_unit{ nullptr };

        uint32_t _sample_rate{ 0 };
        uint32_t _block_size{ 0 };
        uint32_t _channels{ 0 };
    };

} // namespace carrot::audio
