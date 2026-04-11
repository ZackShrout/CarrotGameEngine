//
// Created by zshro on 3/30/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "RHI/Backends/DirectX12/DirectX12Common.h"
#include "RHI/RHI.h"
#include "RHI/Sampler.h"
#include "Renderer/Draw/TexturedQuadBatch.h"

#include <span>

namespace carrot::assets {
    class shader_file_provider_t;
}

namespace carrot::rhi {
    class rhi_buffer_t;
    class rhi_sampler_t;
}

namespace carrot::rhi::dx12 {
    class dx12_texture_t;

    class dx12_textured_quad_sampler_provider_t
    {
    public:
        virtual ~dx12_textured_quad_sampler_provider_t() = default;
        [[nodiscard]] virtual rhi_sampler_t* get_or_create_sampler(const sampler_desc_t& desc) = 0;
    };

    struct draw_context_t
    {
        ID3D12GraphicsCommandList* command_list{ nullptr };

        render_viewport_t viewport{ };

        const rhi_buffer_t* vertex_buffer{ nullptr };
        const rhi_buffer_t* index_buffer{ nullptr };

        std::span<const renderer::textured_quad_batch_t> batches{ };
    };

    struct descriptor_tables_t
    {
        ID3D12DescriptorHeap* srv_heap{ nullptr };
        uint32_t srv_descriptor_size{ 0 };
        D3D12_GPU_DESCRIPTOR_HANDLE camera_cbv_handle{ };
        uint32_t first_batch_srv_index{ 1 };

        ID3D12DescriptorHeap* sampler_heap{ nullptr };
        uint32_t sampler_descriptor_size{ 0 };
        uint32_t first_batch_sampler_index{ 0 };
    };

    struct descriptor_context_t
    {
        descriptor_tables_t tables{ };

        dx12_textured_quad_sampler_provider_t* sampler_provider{ nullptr };
    };

    class dx12_textured_quad_pipeline_t final
    {
    public:
        dx12_textured_quad_pipeline_t(ID3D12Device* device, assets::shader_file_provider_t& shader_files,
                                      std::string_view vertex_shader_path,
                                      std::string_view fragment_shader_path);
        ~dx12_textured_quad_pipeline_t();

        [[nodiscard]] bool is_valid() const noexcept
        {
            return _root_signature != nullptr && _pipeline_state != nullptr;
        }

        void draw(const draw_context_t& draw_context, const descriptor_context_t& descriptor_context) const;

    private:
        void write_batch_descriptors(uint32_t batch_index, const renderer::textured_quad_batch_t& batch,
                                     const descriptor_context_t& descriptor_context) const;

        ID3D12Device* _device{ nullptr };
        ID3D12RootSignature* _root_signature{ nullptr };
        ID3D12PipelineState* _pipeline_state{ nullptr };
    };
} // namespace carrot::rhi::dx12
