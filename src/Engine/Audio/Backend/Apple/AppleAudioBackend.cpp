//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "AppleAudioBackend.h"

#include "../../../Core/CoreDefines.h"

#include <AudioToolbox/AudioToolbox.h>
#include <chlm/CarrotHLM.h>

namespace carrot::audio {
    // PUBLIC

    bool apple_audio_backend_t::init(audio_callback_t* callback, const uint32_t sample_rate, const uint32_t block_size,
                                     const uint32_t channels) noexcept
    {
        LOG_AUDIO_INFO("Initializing Apple Audio Backend...");

        _callback = callback; // callback is owned by audio_engine_t and guaranteed to outlive this backend
        _block_size = block_size; // Requested block size; actual callback size determined by Core Audio
        _channels = channels;

        _client_sample_rate = sample_rate; // sample rate requested by engine
        _hardware_sample_rate = sample_rate; // fallback default

        // Use HAL Output AudioUnit for direct, low-latency device access
        AudioComponentDescription desc{ };
        desc.componentType = kAudioUnitType_Output;
        desc.componentSubType = kAudioUnitSubType_HALOutput;
        desc.componentManufacturer = kAudioUnitManufacturer_Apple;

        AudioComponent component{ AudioComponentFindNext(nullptr, &desc) };
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

        // Bus 0 = output, Bus 1 = input (HAL convention)
        AudioUnitSetProperty(_audio_unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0, &enable,
                             sizeof(enable));
        AudioUnitSetProperty(_audio_unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1, &disable,
                             sizeof(disable));

        // Interleaved float32 PCM expected by audio_engine_t
        AudioStreamBasicDescription format{ };
        format.mSampleRate = static_cast<Float64>(_client_sample_rate);
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

        Float64 auRate{ 0.0 };
        UInt32 size{ sizeof(auRate) };
        OSStatus rateErr{
            AudioUnitGetProperty(_audio_unit, kAudioUnitProperty_SampleRate, kAudioUnitScope_Output, 0, &auRate, &size)
        };

        if (rateErr == noErr && auRate > 0.0)
        {
            _hardware_sample_rate = static_cast<uint32_t>(std::lround(auRate));

            LOG_AUDIO_INFO("Requested client rate: {} Hz. Hardware sample rate: {} Hz", _client_sample_rate,
                           _hardware_sample_rate);
        }
        else
        {
            LOG_AUDIO_WARN("Couldn't get hardware sample rate (OSStatus {}), using fallback {} Hz", rateErr,
                           _hardware_sample_rate);
        }

        LOG_AUDIO_INFO("Apple Audio Backend initialized successfully");

        AudioStreamBasicDescription actualClientFormat{ };
        UInt32 fmtSize = sizeof(actualClientFormat);

        OSStatus fmtErr{
            AudioUnitGetProperty(_audio_unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0,
                                 &actualClientFormat, &fmtSize)
        };

        if (fmtErr == noErr)
        {
            LOG_AUDIO_INFO("Actual AudioUnit client stream format: sampleRate={} channels={} bytesPerFrame={}",
                           actualClientFormat.mSampleRate,
                           actualClientFormat.mChannelsPerFrame,
                           actualClientFormat.mBytesPerFrame);
        }
        else
        {
            LOG_AUDIO_WARN("Failed to query actual AudioUnit client stream format (OSStatus {})", fmtErr);
        }

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
            // Guaranteed no callbacks after AudioOutputUnitStop() / shutdown
            AudioUnitUninitialize(_audio_unit);
            AudioComponentInstanceDispose(_audio_unit);
            _audio_unit = nullptr;
        }

        _callback = nullptr;
    }

    // PRIVATE

    // Core Audio real-time render callback.
    // Must be lock-free, allocation-free, and non-blocking.
    OSStatus apple_audio_backend_t::render_callback(void* in_ref_con,
                                                    [[maybe_unused]] AudioUnitRenderActionFlags* io_action_flags,
                                                    [[maybe_unused]] const AudioTimeStamp* in_time_stamp,
                                                    [[maybe_unused]] UInt32 in_bus_number,
                                                    const UInt32 in_number_frames, AudioBufferList* io_data)
    {
        const auto* self{ static_cast<apple_audio_backend_t *>(in_ref_con) };

        if (!self || !self->_callback || !io_data || io_data->mNumberBuffers == 0 || !io_data->mBuffers[0].mData)
            return noErr;

        float* out{ static_cast<float *>(io_data->mBuffers[0].mData) };

        UInt32 frames_remaining{ in_number_frames };
        UInt32 frame_offset{ 0 };

        while (frames_remaining > 0)
        {
            const UInt32 chunk_frames{ chlm::min<UInt32>(frames_remaining, self->_block_size) };
            float* chunk_out{ out + frame_offset * self->_channels };

            self->_callback->render(chunk_out, chunk_frames, self->_channels);

            frame_offset += chunk_frames;
            frames_remaining -= chunk_frames;
        }

        return noErr;
    }
} // namespace carrot::audio
