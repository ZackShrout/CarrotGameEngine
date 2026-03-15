//
// Created by Zack Shrout on 2/11/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "AudioMixer.h"

#include <cstring>

#include "Audio/DSP/Pan.h"

namespace carrot::audio {
    // PUBLIC
    void audio_mixer_t::init(const uint32_t max_frames, const uint32_t channels) noexcept
    {
        _channels = channels;
        _max_frames = max_frames;
        const uint32_t total{ max_frames * channels };

        for (auto& bus: _buses)
        {
            bus.buffer = new float[total]; // allocated ONCE at init
            bus.gain = 1.f;
        }

        configure_reverb_bus();
        configure_master_bus();
    }

    void audio_mixer_t::shutdown() noexcept
    {
        for (auto& bus: _buses)
        {
            delete[] bus.buffer;
            bus.buffer = nullptr;
        }
    }

    void audio_mixer_t::clear(const uint32_t frame_count) const noexcept
    {
        const uint32_t total{ frame_count * _channels };

        for (auto& bus: _buses)
            std::memset(bus.buffer, 0, total * sizeof(float));
    }

    float* audio_mixer_t::bus_buffer(audio_bus_id id) const noexcept
    {
        return _buses[static_cast<size_t>(id)].buffer;
    }

    float* audio_mixer_t::master_buffer() const noexcept
    {
        return bus_buffer(audio_bus_id::master);
    }

    void audio_mixer_t::mix_bus_into_master(audio_bus_id id, const uint32_t frame_count) const noexcept
    {
        if (id == audio_bus_id::master)
            return;

        const audio_bus_t& src{ _buses[static_cast<size_t>(id)] };
        const audio_bus_t& dst{ _buses[static_cast<size_t>(audio_bus_id::master)] };

        if (src.muted) return;

        if (!src.soloed && any_bus_soloed()) return;

        const uint32_t total{ frame_count * _channels };
        const float gain{ src.gain };

        float pan_l{ 1.f };
        float pan_r{ 1.f };
        compute_pan_gains(src.pan, pan_l, pan_r);

        for (uint32_t frame{ 0 }; frame < frame_count; ++frame)
        {
            const uint32_t i{ frame * _channels };

            dst.buffer[i + 0] += src.buffer[i + 0] * gain * pan_l;
            dst.buffer[i + 1] += src.buffer[i + 1] * gain * pan_r;
        }
    }

    void audio_mixer_t::process_bus_fx(const uint32_t frame_count, const uint32_t sample_rate) const noexcept
    {
        dsp_process_context_t ctx{ };

        ctx.num_channels = _channels;
        ctx.num_frames = frame_count;
        ctx.sample_rate = sample_rate;

        for (size_t i{ 0 }; i < _buses.size(); ++i)
        {
            const audio_bus_t& bus{ _buses[i] };
            const fx_chain_t& chain{ _bus_fx[i] };

            if (static_cast<audio_bus_id>(i) == audio_bus_id::master)
                continue; // master is post-mix only

            if (!bus.buffer || chain.empty())
                continue;

            ctx.interleaved = bus.buffer;
            chain.process(ctx);
        }
    }

    void audio_mixer_t::process_master_fx(const uint32_t frame_count, const uint32_t sample_rate) const noexcept
    {
        dsp_process_context_t ctx{ };
        ctx.num_channels = _channels;
        ctx.num_frames = frame_count;
        ctx.sample_rate = sample_rate;

        const audio_bus_t& bus{ _buses[static_cast<size_t>(audio_bus_id::master)] };
        const fx_chain_t& chain{ _bus_fx[static_cast<size_t>(audio_bus_id::master)] };

        if (!bus.buffer || chain.empty())
            return;

        ctx.interleaved = bus.buffer;
        chain.process(ctx);
    }

    void audio_mixer_t::accumulate_reverb_send(const uint32_t frame_count) const noexcept
    {
        const uint32_t total{ frame_count * _channels };

        const audio_bus_t& reverb_bus{ _buses[static_cast<size_t>(audio_bus_id::reverb)] };

        // Clear reverb buffer before accumulation
        std::memset(reverb_bus.buffer, 0, total * sizeof(float));

        for (size_t i{ 0 }; i < _buses.size(); ++i)
        {
            const audio_bus_id id{ static_cast<audio_bus_id>(i) };

            if (id == audio_bus_id::master || id == audio_bus_id::reverb)
                continue;

            const audio_bus_t& src{ _buses[i] };
            const float send_gain{ src.reverb_send };

            if (send_gain <= 0.f)
                continue;

            const float* src_buf{ src.buffer };
            float* dst_buf{ reverb_bus.buffer };

            for (uint32_t sample{ 0 }; sample < total; ++sample)
                dst_buf[sample] += src_buf[sample] * send_gain;
        }
    }

    // PRIVATE
    bool audio_mixer_t::any_bus_soloed() const noexcept
    {
        for (size_t i{ 0 }; i < _buses.size(); ++i)
        {
            if (static_cast<audio_bus_id>(i) == audio_bus_id::master)
                continue;

            if (_buses[i].soloed)
                return true;
        }

        return false;
    }

    void audio_mixer_t::configure_reverb_bus() noexcept
    {
        _reverb_bus_hp.set_freq(300.f);
        _reverb_bus_hp.set_q(0.707f);
        _reverb_bus_hp.set_gain(0.f);

        _reverb_bus_lp.set_freq(8000.f);
        _reverb_bus_lp.set_q(0.707f);
        _reverb_bus_lp.set_gain(0.f);

        _reverb_bus_verb.set_room_size(0.65f);
        _reverb_bus_verb.set_damp(0.35f);
        _reverb_bus_verb.set_predelay_ms(30.0f);
        _reverb_bus_verb.set_wet(0.40f);
        _reverb_bus_verb.set_dry(0.f);
        _reverb_bus_verb.set_width(1.0f);

        fx_chain_t& reverb_chain{ bus_fx_chain(audio_bus_id::reverb) };

        reverb_chain.clear();
        reverb_chain.add(&_reverb_bus_hp);
        reverb_chain.add(&_reverb_bus_lp);
        reverb_chain.add(&_reverb_bus_verb);
    }

    void audio_mixer_t::configure_master_bus() noexcept
    {
        _master_eq.set_bass_gain_db(1.f);
        _master_eq.set_treble_gain_db(1.5f);
        _master_eq.set_hpf_freq(24.f);
        _master_eq.set_lpf_freq(28000.f);

        _master_bus_comp.set_preset_ssl_style();

        _master_bus_saturator.set_drive(1.2f);
        _master_bus_saturator.set_shape_k(0.5f);
        _master_bus_saturator.set_mix(1.f);
        _master_bus_saturator.set_output_gain(0.9f);

        _master_bus_limiter.set_threshold(0.9f);
        _master_bus_limiter.set_ceiling(0.98f);
        _master_bus_limiter.set_release_ms(120.f);

        fx_chain_t& master_chain{ bus_fx_chain(audio_bus_id::master) };

        master_chain.clear();
        master_chain.add(&_master_eq);
        master_chain.add(&_master_bus_comp);
        master_chain.add(&_master_bus_saturator);
        master_chain.add(&_master_bus_limiter);
    }
} // namespace carrot::audio
