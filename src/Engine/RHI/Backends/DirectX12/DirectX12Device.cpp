//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "DirectX12Device.h"

namespace carrot::rhi::dx12 {
    dx12_device_t::dx12_device_t(const rhi_desc_t& desc)
    {

    }

    dx12_device_t::~dx12_device_t()
    {

    }

    rhi_command_queue_t* dx12_device_t::create_command_queue(queue_type type)
    {
        return nullptr;
    }

    rhi_swapchain_t* dx12_device_t::create_swapchain(uint32_t width, uint32_t height)
    {
        return nullptr;
    }

    rhi_buffer_t* dx12_device_t::create_buffer(const buffer_desc_t& desc)
    {
        return nullptr;
    }

    rhi_texture_t* dx12_device_t::create_texture()
    {
        return nullptr;
    }

    rhi_graphics_pipeline_t* dx12_device_t::create_graphics_pipeline()
    {
        return nullptr;
    }

    void dx12_device_t::destroy_buffer(rhi_buffer_t* buffer)
    {

    }
} // namespace carrot::rhi::dx12
