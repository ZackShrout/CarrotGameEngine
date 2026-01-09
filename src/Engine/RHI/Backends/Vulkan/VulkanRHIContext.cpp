//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySofty. All rights reserved.
//

#include "VulkanRHIContext.h"

namespace carrot::rhi::vulkan {

    vulkan_rhi_context_t::vulkan_rhi_context_t(vulkan::vulkan_renderer_t* existing_renderer)
        : _legacy_renderer(existing_renderer)
    {
        // Extract from legacy
        vulkan_context_t* ctx{ existing_renderer->get_context() };

        _device = std::make_unique<vulkan_device_t>(ctx);

        // We'll create swapchain next step
        // _swapchain = std::make_unique<VulkanSwapchain>(_device.get(), ...);

        _graphics_queue = std::make_unique<vulkan_command_queue_t>(ctx->graphics_queue(), ctx->graphics_family());
    }

    vulkan_rhi_context_t::~vulkan_rhi_context_t() = default;

    void vulkan_rhi_context_t::wait_idle()
    {
        if (_graphics_queue) _graphics_queue->wait_idle();
    }

} // namespace carrot::rhi::vulkan