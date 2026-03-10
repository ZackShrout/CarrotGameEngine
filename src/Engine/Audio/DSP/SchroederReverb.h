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
        enum param_id : dsp_param_id_t
        {
            param_room_size = 0, // 0..1  (tail length)
            param_damp = 1, // 0..1  (HF damping)
            param_wet = 2, // 0..1
            param_width = 3, // 0..1  (stereo width)
            param_predelay_ms = 4 // 0..200ish
        };

        explicit dsp_schroeder_reverb_t(const uint32_t sample_rate, const uint32_t max_predelay_ms = 200) noexcept
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

            for (auto& c: _combs)
            {
                delete[] c.buf_l;
                c.buf_l = nullptr;
                delete[] c.buf_r;
                c.buf_r = nullptr;
            }

            for (auto& a: _allpass)
            {
                delete[] a.buf_l;
                a.buf_l = nullptr;
                delete[] a.buf_r;
                a.buf_r = nullptr;
            }
        }

        void process(dsp_process_context_t& ctx) noexcept override
        {
            if (!ctx.interleaved || ctx.num_channels < 2)
                return;

            const uint32_t frames{ ctx.num_frames };
            float* const x{ ctx.interleaved };

            // Update runtime coefficients (cheap; just a few multiplies)
            const float room{ room_gain() }; // feedback for combs
            const float damp{ _damp }; // 0..1
            const float wet{ _wet }; // 0..1
            const float width{ _width }; // 0..1

            // Mid/side-ish width: wetL = wet*(0.5+0.5*width), wetR = wet*(0.5-0.5*width)
            const float wet1{ wet * (0.5f + 0.5f * width) };
            const float wet2{ wet * (0.5f - 0.5f * width) };

            for (uint32_t f{ 0 }; f < frames; ++f)
            {
                const uint32_t i{ f * 2 };

                const float in_l{ x[i + 0] };
                const float in_r{ x[i + 1] };

                // Mono-ish input to excite reverb (classic trick)
                float input = (in_l + in_r) * 0.5f;

                // Optional predelay
                if (_predelay_samples > 0)
                    input = predelay_process(input);

                // --- Parallel combs (tail) ---
                float acc_l{ 0.f };
                float acc_r{ 0.f };

                for (auto& c: _combs)
                {
                    acc_l += c.process_l(input, room, damp);
                    acc_r += c.process_r(input, room, damp);
                }

                // --- Series allpass (diffusion) ---
                float out_l{ acc_l };
                float out_r{ acc_r };

                for (auto& a: _allpass)
                {
                    out_l = a.process_l(out_l);
                    out_r = a.process_r(out_r);
                }

                // Wet mix with width crossfeed
                const float wet_l{ out_l * wet1 + out_r * wet2 };
                const float wet_r{ out_r * wet1 + out_l * wet2 };

                // Output = dry + wet
                x[i + 0] = in_l * _dry + wet_l;
                x[i + 1] = in_r * _dry + wet_r;
            }
        }

        void set_parameter(uint32_t id, float value) noexcept override
        {
            switch (id)
            {
                case param_room_size:
                    _room_size = chlm::clamp(value, 0.f, 1.f);
                    break;
                case param_damp:
                    _damp = chlm::clamp(value, 0.f, 1.f);
                    break;
                case param_wet:
                    _wet = chlm::clamp(value, 0.f, 1.f);
                    break;
                case param_width:
                    _width = chlm::clamp(value, 0.f, 1.f);
                    break;
                case param_predelay_ms:
                    set_predelay_ms_internal(value);
                    break;
            }
        }

        void reset(const uint32_t sample_rate) noexcept override
        {
            _sample_rate = sample_rate;

            // Clear predelay
            if (_predelay_buf)
            {
                std::memset(_predelay_buf, 0, sizeof(float) * _predelay_capacity);
                _predelay_w = 0;
            }

            // Clear comb/allpass buffers + filter states
            for (auto& c: _combs)
                c.reset();

            for (auto& a: _allpass)
                a.reset();
        }

        // Convenience defaults
        void set_room_size(const float v) noexcept { set_parameter(param_room_size, v); }
        void set_damp(const float v) noexcept { set_parameter(param_damp, v); }
        void set_wet(const float v) noexcept { set_parameter(param_wet, v); }
        void set_dry(const float v) noexcept { _dry = chlm::clamp(v, 0.f, 1.f); }
        void set_width(const float v) noexcept { set_parameter(param_width, v); }
        void set_predelay_ms(const float ms) noexcept { set_parameter(param_predelay_ms, ms); }

    private:
        // ---- building blocks ----

        struct comb_t
        {
            float* buf_l{ nullptr };
            float* buf_r{ nullptr };
            uint32_t len_l{ 1 };
            uint32_t len_r{ 1 };
            uint32_t idx_l{ 0 };
            uint32_t idx_r{ 0 };

            // one-pole damping in feedback path (per channel)
            float filter_l{ 0.f };
            float filter_r{ 0.f };

            // Process one sample
            float process_l(const float input, const float feedback, const float damp) noexcept
            {
                const float y{ buf_l[idx_l] };

                // damping (lowpass in feedback loop)
                filter_l = y * (1.f - damp) + filter_l * damp;
                buf_l[idx_l] = input + filter_l * feedback;

                if (++idx_l >= len_l) idx_l = 0;

                return y;
            }

            float process_r(const float input, const float feedback, const float damp) noexcept
            {
                const float y{ buf_r[idx_r] };

                filter_r = y * (1.f - damp) + filter_r * damp;
                buf_r[idx_r] = input + filter_r * feedback;

                if (++idx_r >= len_r) idx_r = 0;

                return y;
            }

            void reset() noexcept
            {
                if (buf_l) std::memset(buf_l, 0, sizeof(float) * len_l);
                if (buf_r) std::memset(buf_r, 0, sizeof(float) * len_r);

                idx_l = idx_r = 0;
                filter_l = filter_r = 0.f;
            }
        };

        struct allpass_t
        {
            float* buf_l{ nullptr };
            float* buf_r{ nullptr };
            uint32_t len_l{ 1 };
            uint32_t len_r{ 1 };
            uint32_t idx_l{ 0 };
            uint32_t idx_r{ 0 };

            float gain{ 0.5f }; // diffusion amount

            float process_l(const float input) noexcept
            {
                const float bufout{ buf_l[idx_l] };
                const float y{ -input + bufout };

                buf_l[idx_l] = input + bufout * gain;

                if (++idx_l >= len_l) idx_l = 0;

                return y;
            }

            float process_r(const float input) noexcept
            {
                const float bufout{ buf_r[idx_r] };
                const float y{ -input + bufout };

                buf_r[idx_r] = input + bufout * gain;

                if (++idx_r >= len_r) idx_r = 0;

                return y;
            }

            void reset() noexcept
            {
                if (buf_l) std::memset(buf_l, 0, sizeof(float) * len_l);
                if (buf_r) std::memset(buf_r, 0, sizeof(float) * len_r);

                idx_l = idx_r = 0;
            }
        };

        // ---- tuning ----
        // These are “good sounding” classic lengths (similar family to Freeverb),
        // expressed at 44.1k and scaled for actual sample rate.
        static constexpr uint32_t k_num_combs{ 8 };
        static constexpr uint32_t k_num_allpass{ 4 };

        // 44.1k base lengths
        static constexpr uint32_t comb_lengths_L[k_num_combs]{ 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static constexpr uint32_t comb_lengths_R[k_num_combs]{ 1139, 1211, 1300, 1379, 1445, 1514, 1580, 1640 };
        static constexpr uint32_t allpass_lengths_L[k_num_allpass]{ 556, 441, 341, 225 };
        static constexpr uint32_t allpass_lengths_R[k_num_allpass]{ 579, 464, 364, 248 };

        void init_delay_lengths(const uint32_t sample_rate) noexcept
        {
            // scale lengths from 44.1k → sample_rate
            const float scale{ static_cast<float>(sample_rate) / 44100.f };

            for (uint32_t i{ 0 }; i < k_num_combs; ++i)
            {
                comb_t& c{ _combs[i] };

                c.len_l = chlm::max<uint32_t>(1u, static_cast<uint32_t>(comb_lengths_L[i] * scale + 0.5f));
                c.len_r = chlm::max<uint32_t>(1u, static_cast<uint32_t>(comb_lengths_R[i] * scale + 0.5f));

                c.buf_l = new float[c.len_l];
                c.buf_r = new float[c.len_r];
                std::memset(c.buf_l, 0, sizeof(float) * c.len_l);
                std::memset(c.buf_r, 0, sizeof(float) * c.len_r);
            }

            for (uint32_t i = 0; i < k_num_allpass; ++i)
            {
                allpass_t& a{ _allpass[i] };

                a.len_l = chlm::max<uint32_t>(1u, static_cast<uint32_t>(allpass_lengths_L[i] * scale + 0.5f));
                a.len_r = chlm::max<uint32_t>(1u, static_cast<uint32_t>(allpass_lengths_R[i] * scale + 0.5f));

                a.buf_l = new float[a.len_l];
                a.buf_r = new float[a.len_r];
                std::memset(a.buf_l, 0, sizeof(float) * a.len_l);
                std::memset(a.buf_r, 0, sizeof(float) * a.len_r);

                a.gain = 0.5f; // classic diffusion
            }
        }

        // ---- predelay ----
        void init_predelay(const uint32_t sample_rate, const uint32_t max_ms) noexcept
        {
            _predelay_capacity = static_cast<uint32_t>((static_cast<uint64_t>(sample_rate) * max_ms) / 1000ull);
            _predelay_capacity = chlm::max<uint32_t>(1u, _predelay_capacity);
            _predelay_buf = new float[_predelay_capacity];
            std::memset(_predelay_buf, 0, sizeof(float) * _predelay_capacity);
            set_predelay_ms(0.f);
        }

        void set_predelay_ms_internal(float ms) noexcept
        {
            ms = chlm::clamp(ms, 0.f, 250.f);
            _predelay_ms = ms;
            uint32_t s{ static_cast<uint32_t>((static_cast<float>(_sample_rate) * ms) / 1000.f + 0.5f) };

            if (s >= _predelay_capacity) s = _predelay_capacity - 1;

            _predelay_samples = s;
        }

        float predelay_process(const float input) noexcept
        {
            // simple circular delay: write input, read delayed
            const uint32_t read{ (_predelay_w + _predelay_capacity - _predelay_samples) % _predelay_capacity };
            const float out{ _predelay_buf[read] };

            _predelay_buf[_predelay_w] = input;

            if (++_predelay_w >= _predelay_capacity) _predelay_w = 0;

            return out;
        }

        // Map room_size (0..1) to comb feedback gain
        [[nodiscard]] float room_gain() const noexcept
        {
            // Nice range: ~0.7 (small room) to ~0.98 (big hall)
            // Keep < 1 for stability.
            return 0.7f + (_room_size * 0.28f);
        }

        // ---- state ----
        uint32_t _sample_rate{ 48000 };

        comb_t _combs[k_num_combs]{ };
        allpass_t _allpass[k_num_allpass]{ };

        // Predelay
        float* _predelay_buf{ nullptr };
        uint32_t _predelay_capacity{ 1 };
        uint32_t _predelay_w{ 0 };
        float _predelay_ms{ 0.f };
        uint32_t _predelay_samples{ 0 };

        // Parameters
        float _room_size{ 0.5f };
        float _damp{ 0.3f };
        float _wet{ 0.25f };
        float _dry{ 1.f };
        float _width{ 1.f };
    };
} // namespace carrot::audio
