//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "DirectX12Common.h"
#include "DirectX12Core.h"
#include "RHI/RHI.h"

#include <array>
#include <memory>
#include <span>
#include <vector>

namespace carrot::rhi::dx12 {
    class dx12_device_t;
    class dx12_command_queue_t;
    class dx12_swapchain_t;
    class dx12_fence_t;
    class dx12_command_list_t;

    struct dx12_frame_t
    {
        ID3D12CommandAllocator*                  allocator{ nullptr };
        std::unique_ptr<dx12_command_list_t>     command_list;
        std::unique_ptr<dx12_fence_t>            fence;
        uint64_t                                 fence_value{ 0 };
    };

    class dx12_rhi_context_t final : public rhi_context_t
    {
    public:
        explicit dx12_rhi_context_t(const rhi_desc_t& desc);
        ~dx12_rhi_context_t() override;

        void begin_frame() override;
        void record_frame() override;
        void end_frame() override;

        void release_asset_references() override {}

        void resize(uint32_t width, uint32_t height) override;

        [[nodiscard]] rhi_device_t* get_device() const noexcept override;
        [[nodiscard]] rhi_swapchain_t* get_swapchain() const noexcept override;
        [[nodiscard]] rhi_command_queue_t* get_command_queue() const noexcept override;

        [[nodiscard]] std::unique_ptr<rhi_texture_t> create_texture_2d(const texture_create_info_t& info) override;
        [[nodiscard]] std::unique_ptr<rhi_buffer_t> create_buffer(const buffer_create_info_t& info) override;
        [[nodiscard]] std::unique_ptr<rhi_sampler_t> create_sampler(const sampler_desc_t& desc) const override;
        void set_textured_quad_geometry(const rhi_buffer_t& vertex_buffer, const rhi_buffer_t& index_buffer) override {}
        void set_textured_quad_batches(std::span<const renderer::textured_quad_batch_t> batches) override {}

        [[nodiscard]] rhi_sampler_t* get_or_create_sampler(const sampler_desc_t& desc) override;
        void bind_textured_quad_resources(const rhi_texture_t& texture, const rhi_sampler_t& sampler) override {}

        void wait_idle() override;

    private:
        std::unique_ptr<dx12_device_t>                      _device;
        std::unique_ptr<dx12_command_queue_t>               _graphics_queue;
        std::unique_ptr<dx12_swapchain_t>                   _swapchain;

        std::array<dx12_frame_t, k_max_frames_in_flight>    _frames;
        uint32_t                                            _frame_index{ 0 };
        uint32_t                                            _rtv_descriptor_stride{ 0 };

        ID3D12RootSignature*                                _root_signature{ nullptr };
        ID3D12PipelineState*                                _pipeline_state{ nullptr };
        uint32_t                                            _frame_counter{ 0 };

        std::vector<renderer::textured_quad_batch_t> _textured_quad_batches;
    };
} // namespace carrot::rhi::dx12
