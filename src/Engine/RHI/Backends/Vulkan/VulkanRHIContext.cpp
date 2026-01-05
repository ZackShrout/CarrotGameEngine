//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySofty. All rights reserved.
//

#include "VulkanRHIContext.h"

#include "RHI/Device.h"
#include "RHI/Swapchain.h"
#include "RHI/CommandQueue.h"

namespace carrot::rhi {

    vulkan_rhi_context_t::vulkan_rhi_context_t(vulkan::vulkan_renderer_t* existing_renderer)
        : _legacy_renderer(existing_renderer)
    {
        // For now, we just hold the pointer. Real implementations coming soon.
    }

    vulkan_rhi_context_t::~vulkan_rhi_context_t() = default;

    device_t* vulkan_rhi_context_t::get_device() const noexcept
    {
        return nullptr;
        // return _device.get();  // nullptr for now
    }

    swapchain_t* vulkan_rhi_context_t::get_swapchain() const noexcept
    {
        return nullptr;
        // return _swapchain.get();  // nullptr
    }

    command_queue_t* vulkan_rhi_context_t::get_command_queue() const noexcept
    {
        return nullptr;
        // return _graphics_queue.get();  // nullptr
    }

    void vulkan_rhi_context_t::wait_idle()
    {
        // if (_legacy_renderer)
        // {
        //     // Forward to existing Vulkan device wait
        //     vkDeviceWaitIdle(_legacy_renderer->_ctx->device());
        // }
    }

} // namespace carrot::rhi