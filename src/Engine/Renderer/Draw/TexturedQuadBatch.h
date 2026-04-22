//
// Created by Zack Shrout on 3/23/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "QuadSamplerPreset.h"
#include "RHI/Texture.h"

#include <cstdint>

namespace carrot::renderer {
    enum class world_material_domain_t : std::uint8_t
    {
        unlit = 0,
        lit
    };

    struct world_material_key_t
    {
        world_material_domain_t domain{ world_material_domain_t::unlit };
        std::uint32_t feature_flags{ 0u };

        [[nodiscard]] bool operator==(const world_material_key_t& other) const noexcept = default;
    };

    struct textured_quad_batch_t
    {
        const rhi::rhi_texture_t* texture{ nullptr };
        uint32_t first_index{ 0 };
        uint32_t index_count{ 0 };
        uint32_t first_instance{ 0 };
        uint32_t instance_count{ 0 };
        quad_sampler_preset_t sampler_preset{ quad_sampler_preset_t::smooth_clamp };
        world_material_key_t world_material{ };
    };
} // namespace carrot::renderer
