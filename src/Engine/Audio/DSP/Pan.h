//
// Created by Zack Shrout on 2/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <chlm/CarrotHLM.h>

namespace carrot::audio {
    /**
     * @brief Computes equal-power stereo pan gains.
     *
     * Converts a normalized pan value in the range [-1, +1] into
     * left and right channel gain coefficients using equal-power
     * panning.
     *
     * Equal-power panning preserves perceived loudness across
     * the stereo field by using trigonometric gain curves:
     *
     *   left  = cos(theta)
     *   right = sin(theta)
     *
     * where theta is mapped from pan ∈ [-1, +1] to [0, π/2].
     *
     * @param pan   Pan position (-1 = hard left, 0 = center, +1 = hard right)
     * @param out_l Output left channel gain
     * @param out_r Output right channel gain
     *
     * @note
     * This function performs no allocation and is safe for use in
     * real-time audio processing.
     */
    inline void compute_pan_gains(float pan, float& out_l, float& out_r) noexcept
    {
        pan = chlm::clamp(pan, -1.f, 1.f);
        const float angle{ (pan + 1.f) * 0.5f * chlm::pi_half };

        out_l = std::cos(angle);
        out_r = std::sin(angle);
    }
} // namespace carrot::audio
