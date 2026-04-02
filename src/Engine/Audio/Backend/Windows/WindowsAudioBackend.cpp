//
// Created by zshro on 2/21/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "WindowsAudioBackend.h"

#include "Audio/Voice/Voice.h"

#include <avrt.h>

#define WASAPI_CALL(x)                                                      \

    do {                                                                    \
        HRESULT _hr = (x);                                                  \
        if (FAILED(_hr)) {                                                  \
            LOG_AUDIO_ERROR("WASAPI call failed: {} (0x{:08X})", #x, _hr);  \
            return false;                                                   \
        }                                                                   \
    } while (0)


namespace carrot::audio {
    bool windows_audio_backend_t::init(audio_callback_t* callback, [[maybe_unused]] uint32_t requested_sample_rate,
                                       [[maybe_unused]] uint32_t requested_block_size,
                                       [[maybe_unused]] uint32_t requested_channels) noexcept
    {
        _callback = callback;

        WASAPI_CALL(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

        IMMDeviceEnumerator* enumerator{ nullptr };
        WASAPI_CALL(
            CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                reinterpret_cast<void **>(&enumerator)));

        const HRESULT hr{ enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &_device) };
        enumerator->Release();

        if (FAILED(hr)) return false;

        WASAPI_CALL(
            _device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(&_audio_client)));

        WAVEFORMATEX* mix_format{nullptr};
        WASAPI_CALL(_audio_client->GetMixFormat(&mix_format));

        // Validate float32 stereo
        if (mix_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
        {
            if (reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mix_format)->SubFormat != KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
                return false;

            _channels = mix_format->nChannels;
            _sample_rate = mix_format->nSamplesPerSec;
        }

        CE_ASSERT(_channels == 2, "WASAPI backend requires stereo output");

        if (_channels != 2) return false;

        constexpr REFERENCE_TIME buffer_duration{ 10000000 }; // 1 second (100ns units)

        WASAPI_CALL(
            _audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, buffer_duration, 0,
                mix_format, nullptr));

        CoTaskMemFree(mix_format);

        WASAPI_CALL(_audio_client->GetBufferSize(&_buffer_frames));
        WASAPI_CALL(_audio_client->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&_render_client)));

        _buffer_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);

        if (!_buffer_event) return false;

        WASAPI_CALL(_audio_client->SetEventHandle(_buffer_event));

        return true;
    }

    void windows_audio_backend_t::start() noexcept
    {
        _running = true;
        _thread = std::thread(&windows_audio_backend_t::audio_thread_proc, this);
    }
    void windows_audio_backend_t::stop() noexcept
    {
        _running = false;
        SetEvent(_buffer_event); // wake thread

        if (_thread.joinable())
            _thread.join();
    }

    void windows_audio_backend_t::shutdown() noexcept
    {
        if (_render_client)
        {
            _render_client->Release();
            _render_client = nullptr;
        }

        if (_audio_client)
        {
            _audio_client->Release();
            _audio_client = nullptr;
        }

        if (_device)
        {
            _device->Release();
            _device = nullptr;
        }

        if (_buffer_event)
        {
            CloseHandle(_buffer_event);
            _buffer_event = nullptr;
        }

        CoUninitialize();
    }

    void windows_audio_backend_t::audio_thread_proc() const noexcept
    {
        DWORD task_index{ 0 };
        const HANDLE mmcss{ AvSetMmThreadCharacteristicsA("Pro Audio", &task_index) };

        // Prime buffer
        BYTE* primer_data{ nullptr };
        _render_client->GetBuffer(_buffer_frames, &primer_data);

        float* primer_out{ reinterpret_cast<float*>(primer_data) };
        UINT32 frames_remaining{ _buffer_frames };
        UINT32 frame_offset{ 0 };

        while (frames_remaining > 0)
        {
            const UINT32 chunk_frames{ chlm::min<UINT32>(frames_remaining, 512) };
            float* chunk_out{ primer_out + frame_offset * _channels };

            _callback->render(chunk_out, chunk_frames, _channels);

            frame_offset += chunk_frames;
            frames_remaining -= chunk_frames;
        }

        _render_client->ReleaseBuffer(_buffer_frames, 0);
        _audio_client->Start();

        while (_running)
        {
            WaitForSingleObject(_buffer_event, INFINITE);

            UINT32 padding{ 0 };
            _audio_client->GetCurrentPadding(&padding);

            const UINT32 frames_available{ _buffer_frames - padding };

            if (frames_available == 0)
                continue;

            BYTE* data{ nullptr };
            _render_client->GetBuffer(frames_available, &data);

            float* out{ reinterpret_cast<float*>(data) };

            // Render in engine-friendly chunks
            frames_remaining = frames_available;
            frame_offset = 0;

            while (frames_remaining > 0)
            {
                const UINT32 chunk_frames{ chlm::min<UINT32>(frames_remaining, 512) };
                float* chunk_out{ out + frame_offset * _channels };

                _callback->render(chunk_out, chunk_frames, _channels);

                frame_offset += chunk_frames;
                frames_remaining -= chunk_frames;
            }

            _render_client->ReleaseBuffer(frames_available, 0);
        }

        _audio_client->Stop();

        AvRevertMmThreadCharacteristics(mmcss);
    }
} // namespace carrot::audio
