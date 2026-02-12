//
// Created by Zack Shrout on 2/12/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include <chlm/CarrotHLM.h>

namespace carrot::audio {
    inline void compute_pan_gains(const float pan, float& out_l, float& out_r) noexcept
    {
        chlm::clamp(pan, -1.f, 1.f);
        const float angle{ (pan + 1.f) * 0.5f * chlm::pi_half };

        out_l = std::cos(angle);
        out_r = std::sin(angle);
    }
} // namespace carrot::audio
