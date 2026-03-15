//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "NullAudioBackend.h"

#include "../../../Core/CoreDefines.h"

#include <chrono>
#include <vector>

namespace carrot::audio {
    bool null_audio_backend_t::init(
        audio_callback_t* callback,
        const uint32_t sample_rate,
        const uint32_t block_size,
        const uint32_t channels) noexcept
    {
        _callback = callback;
        _sample_rate = sample_rate;
        _block_size = block_size;
        _channels = channels;

        return true;
    }

    void null_audio_backend_t::start() noexcept
    {
        _running = true;
        _thread = std::thread(&null_audio_backend_t::thread_main, this);
    }

    void null_audio_backend_t::stop() noexcept
    {
        _running = false;
        if (_thread.joinable())
            _thread.join();
    }

    void null_audio_backend_t::shutdown() noexcept
    {
        _callback = nullptr;
    }

    uint32_t null_audio_backend_t::sample_rate() const noexcept
    {
        return _sample_rate;
    }

    uint32_t null_audio_backend_t::block_size() const noexcept
    {
        return _block_size;
    }

    uint32_t null_audio_backend_t::channel_count() const noexcept
    {
        return _channels;
    }

    void null_audio_backend_t::thread_main()
    {
        std::vector<float> buffer(_block_size * _channels);

        const auto sleep_time =
                std::chrono::duration<double>(
                    static_cast<double>(_block_size) /
                    static_cast<double>(_sample_rate));

        while (_running)
        {
            _callback->render(
                buffer.data(),
                _block_size,
                _channels);

            // LOG_CORE_TRACE("NullAudioBackend rendered {} frames", _block_size);

            std::this_thread::sleep_for(sleep_time);
        }
    }
} // namespace carrot::audio
