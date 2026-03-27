//
// Created by Zack Shrout on 3/27/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "MetalLayerBridge.h"
#include "Pipelines/MetalTexturedQuadPipeline.h"

namespace carrot::rhi::metal {
    struct descriptor_table_entry_t
    {
        uint64_t gpu_address_or_resource_id;
        uint64_t texture_or_sampler_resource_id;
        uint64_t metadata;
    };

    [[nodiscard]] inline descriptor_table_entry_t encode_metal_texture_srv_descriptor(MTL::Texture* texture) noexcept
    {
        descriptor_table_entry_t entry{ };

        if (!texture)
            return entry;

        const uint64_t resource_id{ metal_texture_resource_id(texture) };

        // NOTE:
        // For the DXC -> Metal converter path used by Carrot, texture SRV descriptors
        // must mirror the Metal texture resource ID into both 64-bit payload fields.
        // Populating only one field caused Texture2D::Load/Sample to resolve as zero.
        entry.gpu_address_or_resource_id = resource_id;
        entry.texture_or_sampler_resource_id = resource_id;
        entry.metadata = 0;

        return entry;
    }

    [[nodiscard]] inline descriptor_table_entry_t encode_metal_sampler_descriptor(MTL::SamplerState* sampler) noexcept
    {
        descriptor_table_entry_t entry{ };

        if (!sampler)
            return entry;

        const uint64_t resource_id{ metal_sampler_resource_id(sampler) };

        entry.gpu_address_or_resource_id = resource_id;
        entry.texture_or_sampler_resource_id = 0;
        entry.metadata = 0;

        return entry;
    }

    [[nodiscard]] inline textured_quad_root_argument_buffer_t encode_textured_quad_root_argument_buffer(
        MTL::Buffer* srv_table_buffer, const size_t srv_table_offset, MTL::Buffer* sampler_table_buffer,
        const size_t sampler_table_offset) noexcept
    {
        textured_quad_root_argument_buffer_t root{ };

        if (srv_table_buffer)
            root.srv_table_gpu_address = metal_buffer_gpu_address(srv_table_buffer) + srv_table_offset;

        if (sampler_table_buffer)
            root.sampler_table_gpu_address = metal_buffer_gpu_address(sampler_table_buffer) + sampler_table_offset;

        return root;
    }
} // namespace carrot::rhi::metal
