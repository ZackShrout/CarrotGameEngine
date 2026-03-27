//
// Created by Zack Shrout on 3/24/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "RHI/Backends/Metal/MetalCommon.h"

#include <memory>

namespace carrot::assets {
    class shader_file_provider_t;
}

namespace carrot::rhi::metal {
    class metal_device_t;

    struct descriptor_table_entry_t
    {
        uint64_t gpu_address_or_resource_id;
        uint64_t texture_or_sampler_resource_id;
        uint64_t metadata;
    };

    struct textured_quad_root_argument_buffer_t
    {
        uint64_t srv_table_gpu_address;
        uint64_t sampler_table_gpu_address;
    };

    class metal_textured_quad_pipeline_t final
    {
    public:
        metal_textured_quad_pipeline_t(const metal_device_t& device, assets::shader_file_provider_t& shader_files,
                                       MTL::PixelFormat color_format);

        ~metal_textured_quad_pipeline_t();

        metal_textured_quad_pipeline_t(const metal_textured_quad_pipeline_t&) = delete;
        metal_textured_quad_pipeline_t& operator=(const metal_textured_quad_pipeline_t&) = delete;

        [[nodiscard]] bool is_valid() const noexcept;

        [[nodiscard]] MTL::RenderPipelineState* state() const noexcept;
        [[nodiscard]] MTL::VertexDescriptor* vertex_descriptor() const noexcept;

    private:
        [[nodiscard]] static MTL::VertexDescriptor* create_vertex_descriptor();
        [[nodiscard]] static MTL::Library* load_library(MTL::Device* device,
                                                        const assets::shader_file_provider_t& shader_files,
                                                        std::string_view virtual_path);

        [[nodiscard]] static MTL::Function* load_function(MTL::Library* library);

        std::shared_ptr<MTL::RenderPipelineState> _state;
        std::shared_ptr<MTL::VertexDescriptor> _vertex_descriptor;
    };
} // namespace carrot::rhi::metal
