//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "VulkanCommon.h"
#include "VulkanCore.h"
#include "RHI/Device.h"

namespace carrot::rhi::vulkan {
    class vulkan_device_t final : public rhi_device_t
    {
    public:
        explicit vulkan_device_t(VkDevice device, VkPhysicalDevice physical_device, uint32_t graphics_family,
                                 VkQueue graphics_queue, VkSurfaceKHR surface);

        ~vulkan_device_t() override;

        rhi_command_queue_t* create_command_queue(queue_type queue) override;
        rhi_swapchain_t* create_swapchain(uint32_t width, uint32_t height) override;

        rhi_buffer_t* create_buffer(const buffer_desc_t& desc) override;
        rhi_texture_t* create_texture() override;
        rhi_graphics_pipeline_t* create_graphics_pipeline() override;

        void destroy_buffer(rhi_buffer_t* buffer) override;

        // Accessors for internal use
        [[nodiscard]] VkDevice vk_device() const noexcept { return _device.device; }
        [[nodiscard]] VkPhysicalDevice physical_device() const noexcept { return _physical_device; }
        [[nodiscard]] uint32_t graphics_family() const noexcept { return _graphics_family; }
        [[nodiscard]] VkSurfaceKHR surface() const noexcept { return _surface; }
        [[nodiscard]] VkQueue graphics_queue() const noexcept { return _graphics_queue; }

    private:
        device_t            _device;
        VkPhysicalDevice    _physical_device{ VK_NULL_HANDLE };
        VkSurfaceKHR        _surface{ VK_NULL_HANDLE };
        uint32_t            _graphics_family{ ~0u };
        VkQueue             _graphics_queue{ VK_NULL_HANDLE };
    };
} // namespace carrot::rhi::vulkan
