//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "VulkanDevice.h"

#include "VulkanCommandQueue.h"
#include "VulkanSwapchain.h"

namespace carrot::rhi::vulkan {
    vulkan_device_t::vulkan_device_t(VkDevice device, VkPhysicalDevice physical_device, const uint32_t graphics_family,
                                     VkQueue graphics_queue, VkSurfaceKHR surface)
        : _device{ device }, _physical_device{ physical_device }, _surface{ surface },
          _graphics_family{ graphics_family }, _graphics_queue{ graphics_queue }
    {
        LOG_GRAPHICS_INFO("VulkanDevice constructed with fresh VkDevice: {:p}", static_cast<void *>(_device));
    }

    vulkan_device_t::~vulkan_device_t() = default;

    rhi_command_queue_t* vulkan_device_t::create_command_queue(queue_type queue)
    {
        if (queue != queue_type::graphics)
        {
            LOG_GRAPHICS_WARN("Only graphics queue supported for now");
        }

        // We'll create real one soon — for now return a dummy
        static vulkan_command_queue_t dummy(_graphics_queue, _graphics_family);
        return &dummy;
    }

    rhi_swapchain_t* vulkan_device_t::create_swapchain(const uint32_t width, const uint32_t height)
    {
        return new vulkan_swapchain_t{ this, _surface, width, height };
    }

    rhi_buffer_t* vulkan_device_t::create_buffer(const buffer_desc_t& desc)
    {
        return nullptr; // temporary
    }

    rhi_texture_t* vulkan_device_t::create_texture()
    {
        return nullptr; // temporary
    }

    rhi_graphics_pipeline_t* vulkan_device_t::create_graphics_pipeline()
    {
        return nullptr; // temporary
    }

    void vulkan_device_t::destroy_buffer(rhi_buffer_t* buffer) {}
} // namespace carrot::rhi::vulkan
