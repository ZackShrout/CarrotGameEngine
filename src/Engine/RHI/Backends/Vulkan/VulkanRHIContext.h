//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySofty. All rights reserved.
//

#pragma once

#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include "VulkanCommandQueue.h"
#include "RHI/RHI.h"
#include "RHI/Backends/Vulkan/VulkanRenderer.h"  // Keep access to your existing renderer

namespace carrot::rhi::vulkan {

    class vulkan_rhi_context_t final : public rhi_context_t
    {
    public:
        explicit vulkan_rhi_context_t(vulkan_renderer_t* existing_renderer);
        ~vulkan_rhi_context_t() override;

        [[nodiscard]] rhi_device_t*         get_device() const noexcept override { return _device.get(); }
        [[nodiscard]] rhi_swapchain_t*      get_swapchain() const noexcept override { return _swapchain.get();}
        [[nodiscard]] rhi_command_queue_t*  get_command_queue() const noexcept override { return _graphics_queue.get(); }

        void wait_idle() override;

    private:
        vulkan_renderer_t* _legacy_renderer{ nullptr };

        std::unique_ptr<vulkan_device_t>        _device;
        std::unique_ptr<vulkan_swapchain_t>     _swapchain;
        std::unique_ptr<vulkan_command_queue_t> _graphics_queue;
    };

} // namespace carrot::rhi::vulkan