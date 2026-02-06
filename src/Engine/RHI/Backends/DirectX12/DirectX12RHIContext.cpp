//
// Created by zshro on 2/4/2026.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "DirectX12RHIContext.h"

#include "RHI/RHI.h"

namespace carrot::rhi::dx12 {
    dx12_rhi_context_t::dx12_rhi_context_t(const rhi_desc_t& desc)
    {

    }

    dx12_rhi_context_t::~dx12_rhi_context_t()
    {

    }

    void dx12_rhi_context_t::begin_frame()
    {

    }

    void dx12_rhi_context_t::record_frame()
    {

    }

    void dx12_rhi_context_t::end_frame()
    {

    }

    void dx12_rhi_context_t::resize(uint32_t width, uint32_t height)
    {

    }

    rhi_device_t* dx12_rhi_context_t::get_device() const noexcept
    {
        return nullptr;
    }

    rhi_swapchain_t* dx12_rhi_context_t::get_swapchain() const noexcept
    {
        return nullptr;
    }

    rhi_command_queue_t* dx12_rhi_context_t::get_command_queue() const noexcept
    {
        return nullptr;
    }

    void dx12_rhi_context_t::wait_idle()
    {

    }
} // namespace carrot::rhi::dx12
