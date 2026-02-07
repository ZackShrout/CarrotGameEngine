//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "DirectX12Common.h"
#include "RHI/Device.h"

namespace carrot::rhi {
    struct rhi_desc_t;
}

namespace carrot::rhi::dx12 {
    class dx12_device_t final : public rhi_device_t
    {
    public:
        explicit dx12_device_t(const rhi_desc_t& desc);
        ~dx12_device_t() override;

        rhi_command_queue_t* create_command_queue(queue_type type) override;
        rhi_swapchain_t* create_swapchain(uint32_t width, uint32_t height) override;

        rhi_buffer_t* create_buffer(const buffer_desc_t& desc) override;
        rhi_texture_t* create_texture() override;
        rhi_graphics_pipeline_t* create_graphics_pipeline() override;

        void destroy_buffer(rhi_buffer_t* buffer) override;

        // Accessors for internal use
        [[nodiscard]] ID3D12Device* id3d12_device() const noexcept { return _device; }

    private:
        ID3D12Device* _device{ nullptr };
    };
} // namespace carrot::rhi::dx12
