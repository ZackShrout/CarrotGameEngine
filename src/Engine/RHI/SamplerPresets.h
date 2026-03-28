//
// Created by Zack Shrout on 3/27/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "RHI/Sampler.h"
#include "Renderer/Draw/QuadSamplerPreset.h"

namespace carrot::rhi {
    [[nodiscard]] inline sampler_desc_t sampler_desc_from_preset(const renderer::quad_sampler_preset_t preset) noexcept
    {
        sampler_desc_t desc{ };

        switch (preset)
        {
            case renderer::quad_sampler_preset_t::pixel_clamp:
                desc.min_filter = sampler_filter_t::nearest;
                desc.mag_filter = sampler_filter_t::nearest;
                desc.mip_filter = sampler_mip_filter_t::nearest;
                desc.address_u = sampler_address_mode_t::clamp_to_edge;
                desc.address_v = sampler_address_mode_t::clamp_to_edge;
                desc.address_w = sampler_address_mode_t::clamp_to_edge;
                break;

            case renderer::quad_sampler_preset_t::smooth_clamp:
                desc.min_filter = sampler_filter_t::linear;
                desc.mag_filter = sampler_filter_t::linear;
                desc.mip_filter = sampler_mip_filter_t::linear;
                desc.address_u = sampler_address_mode_t::clamp_to_edge;
                desc.address_v = sampler_address_mode_t::clamp_to_edge;
                desc.address_w = sampler_address_mode_t::clamp_to_edge;
                break;

            case renderer::quad_sampler_preset_t::pixel_repeat:
                desc.min_filter = sampler_filter_t::nearest;
                desc.mag_filter = sampler_filter_t::nearest;
                desc.mip_filter = sampler_mip_filter_t::nearest;
                desc.address_u = sampler_address_mode_t::repeat;
                desc.address_v = sampler_address_mode_t::repeat;
                desc.address_w = sampler_address_mode_t::repeat;
                break;

            case renderer::quad_sampler_preset_t::smooth_repeat:
                desc.min_filter = sampler_filter_t::linear;
                desc.mag_filter = sampler_filter_t::linear;
                desc.mip_filter = sampler_mip_filter_t::linear;
                desc.address_u = sampler_address_mode_t::repeat;
                desc.address_v = sampler_address_mode_t::repeat;
                desc.address_w = sampler_address_mode_t::repeat;
                break;
        }

        return desc;
    }
} // namespace carrot::rhi
