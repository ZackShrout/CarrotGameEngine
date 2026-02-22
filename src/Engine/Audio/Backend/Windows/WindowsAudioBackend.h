//
// Created by zshro on 2/21/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "Audio/Backend/AudioBackend.h"

#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <thread>

namespace carrot::audio {
    class windows_audio_backend_t final : public audio_backend_t
    {
    public:
        bool init(audio_callback_t* callback, uint32_t requested_sample_rate, uint32_t requested_block_size,
                  uint32_t requested_channels) noexcept override;

        void start() noexcept override;
        void stop() noexcept override;
        void shutdown() noexcept override;

        uint32_t sample_rate() const noexcept override { return _sample_rate; }
        uint32_t block_size()  const noexcept override { return _buffer_frames; }
        uint32_t channel_count() const noexcept override { return _channels; }

    private:
        void audio_thread_proc() noexcept;

        audio_callback_t* _callback{ nullptr };

        IMMDevice* _device{ nullptr };
        IAudioClient* _audio_client{ nullptr };
        IAudioRenderClient* _render_client{ nullptr };

        HANDLE _buffer_event{ nullptr };
        std::thread _thread;
        std::atomic<bool> _running{ false };

        uint32_t _sample_rate{ 0 };
        uint32_t _channels{ 0 };
        uint32_t _buffer_frames{ 0 };

        double _phase{ 0.0 }; // sine test
    };
} // namespace carrot::audio
