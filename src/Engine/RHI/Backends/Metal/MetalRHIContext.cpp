//
// Created by Zack Shrout on 2/2/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "MetalRHIContext.h"

namespace carrot::rhi::metal {
    metal_rhi_context_t::metal_rhi_context_t(const rhi_desc_t& desc)
    {

    }

    metal_rhi_context_t::~metal_rhi_context_t()
    {

    }

    void metal_rhi_context_t::begin_frame()
    {

    }

    void metal_rhi_context_t::record_frame()
    {

    }

    void metal_rhi_context_t::end_frame()
    {

    }

    void metal_rhi_context_t::resize(uint32_t width, uint32_t height)
    {

    }

    rhi_device_t* metal_rhi_context_t::get_device() const noexcept
    {
        return nullptr;
    }

    rhi_swapchain_t* metal_rhi_context_t::get_swapchain() const noexcept
    {
        return nullptr;
    }

    rhi_command_queue_t* metal_rhi_context_t::get_command_queue() const noexcept
    {
        return nullptr;
    }

    void metal_rhi_context_t::wait_idle()
    {

    }
} // namespace carrot::rhi::metal