//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "AppleAudioBackend.h"

#include "Common/CommonHeaders.h"

namespace carrot::audio {
    bool apple_audio_backend_t::init(audio_callback_t* callback, uint32_t sample_rate, uint32_t block_size,
                                     uint32_t channels) noexcept
    {
        LOG_AUDIO_INFO("Initializing Apple Audio Backend...");

        _callback = callback;
        _sample_rate = sample_rate;
        _block_size = block_size;
        _channels = channels;

        // ── Describe HAL Output AudioUnit ───────────────────────────
        AudioComponentDescription desc{ };
        desc.componentType = kAudioUnitType_Output;
        desc.componentSubType = kAudioUnitSubType_HALOutput;
        desc.componentManufacturer = kAudioUnitManufacturer_Apple;

        const AudioComponent component{ AudioComponentFindNext(nullptr, &desc) };
        if (!component)
        {
            LOG_AUDIO_FATAL("Failed to find HAL Output AudioUnit");
            return false;
        }

        if (AudioComponentInstanceNew(component, &_audio_unit) != noErr)
        {
            LOG_AUDIO_FATAL("Failed to create AudioUnit instance");
            return false;
        }

        // Enable output, disable input
        constexpr UInt32 enable{ 1 };
        constexpr UInt32 disable{ 0 };

        AudioUnitSetProperty(_audio_unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0, &enable,
                             sizeof(enable));
        AudioUnitSetProperty(_audio_unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1, &disable,
                             sizeof(disable));

        // ── Stream format ───────────────────────────────────────────
        AudioStreamBasicDescription format{ };
        format.mSampleRate = static_cast<Float64>(_sample_rate);
        format.mFormatID = kAudioFormatLinearPCM;
        format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
        format.mBitsPerChannel = 32;
        format.mChannelsPerFrame = _channels;
        format.mFramesPerPacket = 1;
        format.mBytesPerFrame = sizeof(float) * _channels;
        format.mBytesPerPacket = format.mBytesPerFrame;

        if (AudioUnitSetProperty(_audio_unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &format,
                                 sizeof(format)) != noErr)
        {
            LOG_AUDIO_FATAL("Failed to set AudioUnit stream format");
            return false;
        }

        // ── Render callback ─────────────────────────────────────────
        AURenderCallbackStruct cb{ };
        cb.inputProc = &apple_audio_backend_t::render_callback;
        cb.inputProcRefCon = this;

        if (AudioUnitSetProperty(_audio_unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &cb,
                                 sizeof(cb)) != noErr)
        {
            LOG_AUDIO_FATAL("Failed to set AudioUnit render callback");
            return false;
        }

        if (AudioUnitInitialize(_audio_unit) != noErr)
        {
            LOG_AUDIO_FATAL("Failed to initialize AudioUnit");
            return false;
        }

        LOG_AUDIO_INFO("Apple Audio Backend initialized successfully");

        return true;
    }

    void apple_audio_backend_t::start() noexcept
    {
        AudioOutputUnitStart(_audio_unit);
    }

    void apple_audio_backend_t::stop() noexcept
    {
        AudioOutputUnitStop(_audio_unit);
    }

    void apple_audio_backend_t::shutdown() noexcept
    {
        if (_audio_unit)
        {
            AudioUnitUninitialize(_audio_unit);
            AudioComponentInstanceDispose(_audio_unit);
            _audio_unit = nullptr;
        }

        _callback = nullptr;
    }

    OSStatus apple_audio_backend_t::render_callback(void* in_ref_con,
                                                    [[maybe_unused]] AudioUnitRenderActionFlags* io_action_flags,
                                                    [[maybe_unused]] const AudioTimeStamp* in_time_stamp,
                                                    [[maybe_unused]] UInt32 in_bus_number,
                                                    const UInt32 in_number_frames, AudioBufferList* io_data)
    {
        const apple_audio_backend_t* self{ static_cast<apple_audio_backend_t *>(in_ref_con) };
        float* output{ static_cast<float *>(io_data->mBuffers[0].mData) };
        self->_callback->render(output, in_number_frames, self->_channels);

        return noErr;
    }
} // namespace carrot::audio
