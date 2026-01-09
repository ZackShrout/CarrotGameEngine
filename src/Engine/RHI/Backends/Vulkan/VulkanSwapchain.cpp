//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySofty. All rights reserved.
//

#include "VulkanSwapchain.h"

namespace carrot::rhi::vulkan {
    vulkan_swapchain_t::vulkan_swapchain_t(vulkan_device_t* device, void* native_window, uint32_t width,
        uint32_t height)
    {

    }

    vulkan_swapchain_t::~vulkan_swapchain_t()
    {

    }

    void vulkan_swapchain_t::resize(uint32_t width, uint32_t height)
    {

    }

    uint32_t vulkan_swapchain_t::acquire_next_image(rhi_semaphore_t* signal_semaphore)
    {
        return 0; // temporary
    }

    void vulkan_swapchain_t::present(rhi_semaphore_t* wait_semaphore)
    {

    }
} // namespace carrot::rhi::vulkan