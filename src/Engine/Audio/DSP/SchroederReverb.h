//
// Created by Zack Shrout on 2/25/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "DspUnit.h"

#include <cstring>
#include <algorithm>

namespace carrot::audio {

    class dsp_schroeder_reverb_t final : public dsp_unit_t
    {
    public:
        enum param_id : uint32_t
        {
            param_room_size = 0,   // 0..1  (tail length)
            param_damp      = 1,   // 0..1  (HF damping)
            param_wet       = 2,   // 0..1
            param_width     = 3,   // 0..1  (stereo width)
            param_predelay_ms = 4  // 0..200ish
        };

        dsp_schroeder_reverb_t(uint32_t sample_rate, uint32_t max_predelay_ms = 200) noexcept
            : _sample_rate(sample_rate)
        {
            init_delay_lengths(sample_rate);
            init_predelay(sample_rate, max_predelay_ms);
            reset(sample_rate);
        }

        ~dsp_schroeder_reverb_t() override
        {
            delete[] _predelay_buf;
            _predelay_buf = nullptr;

            for (auto& c : _combs)
            {
                delete[] c.bufL; c.bufL = nullptr;
                delete[] c.bufR; c.bufR = nullptr;
            }

            for (auto& a : _allpass)
            {
                delete[] a.bufL; a.bufL = nullptr;
                delete[] a.bufR; a.bufR = nullptr;
            }
        }

        void process(dsp_process_context_t& ctx) noexcept override
        {
            if (!ctx.interleaved || ctx.num_channels < 2)
                return;

            const uint32_t frames = ctx.num_frames;
            float* const x = ctx.interleaved;

            // Update runtime coefficients (cheap; just a few multiplies)
            const float room = room_gain();         // feedback for combs
            const float damp = _damp;               // 0..1
            const float wet  = _wet;                // 0..1
            const float width = _width;             // 0..1

            // Mid/side-ish width: wetL = wet*(0.5+0.5*width), wetR = wet*(0.5-0.5*width)
            const float wet1 = wet * (0.5f + 0.5f * width);
            const float wet2 = wet * (0.5f - 0.5f * width);

            for (uint32_t f = 0; f < frames; ++f)
            {
                const uint32_t i = f * 2;

                float inL = x[i + 0];
                float inR = x[i + 1];

                // Mono-ish input to excite reverb (classic trick)
                float input = (inL + inR) * 0.5f;

                // Optional predelay
                if (_predelay_samples > 0)
                    input = predelay_process(input);

                // --- Parallel combs (tail) ---
                float accL = 0.0f;
                float accR = 0.0f;

                for (auto& c : _combs)
                {
                    accL += c.processL(input, room, damp);
                    accR += c.processR(input, room, damp);
                }

                // --- Series allpass (diffusion) ---
                float outL = accL;
                float outR = accR;

                for (auto& a : _allpass)
                {
                    outL = a.processL(outL);
                    outR = a.processR(outR);
                }

                // Wet mix with width crossfeed
                const float wetL = outL * wet1 + outR * wet2;
                const float wetR = outR * wet1 + outL * wet2;

                // Output = dry + wet
                x[i + 0] = inL * _dry + wetL;
                x[i + 1] = inR * _dry + wetR;
            }
        }

        void set_parameter(uint32_t id, float value) noexcept override
        {
            switch (id)
            {
                case param_room_size:
                    _room_size = std::clamp(value, 0.0f, 1.0f);
                    break;
                case param_damp:
                    _damp = std::clamp(value, 0.0f, 1.0f);
                    break;
                case param_wet:
                    _wet = std::clamp(value, 0.0f, 1.0f);
                    break;
                case param_width:
                    _width = std::clamp(value, 0.0f, 1.0f);
                    break;
                case param_predelay_ms:
                    set_predelay_ms_internal(value);
                    break;
            }
        }

        void reset(uint32_t sample_rate) noexcept override
        {
            _sample_rate = sample_rate;

            // Clear predelay
            if (_predelay_buf)
            {
                std::memset(_predelay_buf, 0, sizeof(float) * _predelay_capacity);
                _predelay_w = 0;
            }

            // Clear comb/allpass buffers + filter states
            for (auto& c : _combs)
                c.reset();

            for (auto& a : _allpass)
                a.reset();
        }

        // Convenience defaults
        void set_room_size(float v) noexcept { set_parameter(param_room_size, v); }
        void set_damp(float v) noexcept      { set_parameter(param_damp, v); }
        void set_wet(float v) noexcept       { set_parameter(param_wet, v); }
        void set_dry(float v) noexcept       { _dry = std::clamp(v, 0.0f, 1.0f); }
        void set_width(float v) noexcept     { set_parameter(param_width, v); }
        void set_predelay_ms(float ms) noexcept { set_parameter(param_predelay_ms, ms); }

    private:
        // ---- building blocks ----

        struct comb_t
        {
            float* bufL{ nullptr };
            float* bufR{ nullptr };
            uint32_t lenL{ 1 };
            uint32_t lenR{ 1 };
            uint32_t idxL{ 0 };
            uint32_t idxR{ 0 };

            // one-pole damping in feedback path (per channel)
            float filterL{ 0.0f };
            float filterR{ 0.0f };

            // Process one sample
            float processL(float input, float feedback, float damp) noexcept
            {
                float y = bufL[idxL];

                // damping (lowpass in feedback loop)
                filterL = (y * (1.0f - damp)) + (filterL * damp);

                bufL[idxL] = input + filterL * feedback;

                if (++idxL >= lenL) idxL = 0;
                return y;
            }

            float processR(float input, float feedback, float damp) noexcept
            {
                float y = bufR[idxR];
                filterR = (y * (1.0f - damp)) + (filterR * damp);
                bufR[idxR] = input + filterR * feedback;
                if (++idxR >= lenR) idxR = 0;
                return y;
            }

            void reset() noexcept
            {
                if (bufL) std::memset(bufL, 0, sizeof(float) * lenL);
                if (bufR) std::memset(bufR, 0, sizeof(float) * lenR);
                idxL = idxR = 0;
                filterL = filterR = 0.0f;
            }
        };

        struct allpass_t
        {
            float* bufL{ nullptr };
            float* bufR{ nullptr };
            uint32_t lenL{ 1 };
            uint32_t lenR{ 1 };
            uint32_t idxL{ 0 };
            uint32_t idxR{ 0 };

            float gain{ 0.5f }; // diffusion amount

            float processL(float input) noexcept
            {
                const float bufout = bufL[idxL];
                const float y = -input + bufout;
                bufL[idxL] = input + bufout * gain;
                if (++idxL >= lenL) idxL = 0;
                return y;
            }

            float processR(float input) noexcept
            {
                const float bufout = bufR[idxR];
                const float y = -input + bufout;
                bufR[idxR] = input + bufout * gain;
                if (++idxR >= lenR) idxR = 0;
                return y;
            }

            void reset() noexcept
            {
                if (bufL) std::memset(bufL, 0, sizeof(float) * lenL);
                if (bufR) std::memset(bufR, 0, sizeof(float) * lenR);
                idxL = idxR = 0;
            }
        };

        // ---- tuning ----
        // These are “good sounding” classic lengths (similar family to Freeverb),
        // expressed at 44.1k and scaled for actual sample rate.
        static constexpr uint32_t k_num_combs   = 8;
        static constexpr uint32_t k_num_allpass = 4;

        // 44.1k base lengths
        static constexpr uint32_t comb_lengths_L[k_num_combs]   = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static constexpr uint32_t comb_lengths_R[k_num_combs]   = { 1139, 1211, 1300, 1379, 1445, 1514, 1580, 1640 };
        static constexpr uint32_t allpass_lengths_L[k_num_allpass] = { 556, 441, 341, 225 };
        static constexpr uint32_t allpass_lengths_R[k_num_allpass] = { 579, 464, 364, 248 };

        void init_delay_lengths(uint32_t sample_rate) noexcept
        {
            // scale lengths from 44.1k → sample_rate
            const float scale = static_cast<float>(sample_rate) / 44100.0f;

            for (uint32_t i = 0; i < k_num_combs; ++i)
            {
                auto& c = _combs[i];

                c.lenL = std::max<uint32_t>(1u, static_cast<uint32_t>(comb_lengths_L[i] * scale + 0.5f));
                c.lenR = std::max<uint32_t>(1u, static_cast<uint32_t>(comb_lengths_R[i] * scale + 0.5f));

                c.bufL = new float[c.lenL];
                c.bufR = new float[c.lenR];
                std::memset(c.bufL, 0, sizeof(float) * c.lenL);
                std::memset(c.bufR, 0, sizeof(float) * c.lenR);
            }

            for (uint32_t i = 0; i < k_num_allpass; ++i)
            {
                auto& a = _allpass[i];

                a.lenL = std::max<uint32_t>(1u, static_cast<uint32_t>(allpass_lengths_L[i] * scale + 0.5f));
                a.lenR = std::max<uint32_t>(1u, static_cast<uint32_t>(allpass_lengths_R[i] * scale + 0.5f));

                a.bufL = new float[a.lenL];
                a.bufR = new float[a.lenR];
                std::memset(a.bufL, 0, sizeof(float) * a.lenL);
                std::memset(a.bufR, 0, sizeof(float) * a.lenR);

                a.gain = 0.5f; // classic diffusion
            }
        }

        // ---- predelay ----
        void init_predelay(uint32_t sample_rate, uint32_t max_ms) noexcept
        {
            _predelay_capacity = static_cast<uint32_t>((static_cast<uint64_t>(sample_rate) * max_ms) / 1000ull);
            _predelay_capacity = std::max<uint32_t>(1u, _predelay_capacity);
            _predelay_buf = new float[_predelay_capacity];
            std::memset(_predelay_buf, 0, sizeof(float) * _predelay_capacity);
            set_predelay_ms(0.0f);
        }

        void set_predelay_ms_internal(float ms) noexcept
        {
            ms = std::clamp(ms, 0.0f, 250.0f);
            _predelay_ms = ms;
            uint32_t s = static_cast<uint32_t>((static_cast<float>(_sample_rate) * ms) / 1000.0f + 0.5f);
            if (s >= _predelay_capacity) s = _predelay_capacity - 1;
            _predelay_samples = s;
        }

        float predelay_process(float input) noexcept
        {
            // simple circular delay: write input, read delayed
            const uint32_t read = (_predelay_w + _predelay_capacity - _predelay_samples) % _predelay_capacity;
            const float out = _predelay_buf[read];
            _predelay_buf[_predelay_w] = input;
            if (++_predelay_w >= _predelay_capacity) _predelay_w = 0;
            return out;
        }

        // Map room_size (0..1) to comb feedback gain
        float room_gain() const noexcept
        {
            // Nice range: ~0.7 (small room) to ~0.98 (big hall)
            // Keep < 1 for stability.
            return 0.70f + (_room_size * 0.28f);
        }

        // ---- state ----
        uint32_t _sample_rate{ 48000 };

        comb_t    _combs[k_num_combs]{};
        allpass_t _allpass[k_num_allpass]{};

        // Predelay
        float*   _predelay_buf{ nullptr };
        uint32_t _predelay_capacity{ 1 };
        uint32_t _predelay_w{ 0 };
        float    _predelay_ms{ 0.0f };
        uint32_t _predelay_samples{ 0 };

        // Parameters
        float _room_size{ 0.5f };
        float _damp{ 0.3f };
        float _wet{ 0.25f };
        float _dry{ 1.0f };
        float _width{ 1.0f };
    };

} // namespace carrot::audio