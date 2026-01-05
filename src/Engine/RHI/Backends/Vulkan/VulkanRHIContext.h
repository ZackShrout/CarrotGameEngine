//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySofty. All rights reserved.
//

#pragma once

#include "RHI/RHI.h"
#include "RHI/Backends/Vulkan/VulkanRenderer.h"  // Keep access to your existing renderer

namespace carrot::rhi {

    class vulkan_rhi_context_t final : public rhi_context_t
    {
    public:
        explicit vulkan_rhi_context_t(vulkan::vulkan_renderer_t* existing_renderer);
        ~vulkan_rhi_context_t() override;

        [[nodiscard]] device_t*         get_device() const noexcept override;
        [[nodiscard]] swapchain_t*      get_swapchain() const noexcept override;
        [[nodiscard]] command_queue_t*  get_command_queue() const noexcept override;

        void wait_idle() override;

    private:
        vulkan::vulkan_renderer_t* _legacy_renderer{ nullptr };

        // We'll fill these in later as real objects
        // class vulkan_device_t;
        // class vulkan_swapchain_t;
        // class vulkan_command_queue_t;
        //
        // std::unique_ptr<vulkan_device_t>        _device;
        // std::unique_ptr<vulkan_swapchain_t>     _swapchain;
        // std::unique_ptr<vulkan_command_queue_t> _graphics_queue;
    };

} // namespace carrot::rhi