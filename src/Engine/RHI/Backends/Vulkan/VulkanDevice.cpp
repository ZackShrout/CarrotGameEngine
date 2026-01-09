//
// Created by zshrout on 1/4/26.
// Copyright (c) 2026 BunnySofty. All rights reserved.
//

#include "VulkanDevice.h"

#include "VulkanCommandQueue.h"

namespace carrot::rhi::vulkan {
    vulkan_device_t::vulkan_device_t(vulkan_context_t* legacy_context)
    {
        // Steal/move the device ownership
        // _device = std::move(legacy_context->get_device());

        _physical_device = legacy_context->physical_device();
        _graphics_family = legacy_context->graphics_family();
        _graphics_queue = legacy_context->graphics_queue();

        _device_handle     = legacy_context->get_device();
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

    rhi_swapchain_t* vulkan_device_t::create_swapchain(void* native_window, uint32_t width, uint32_t height)
    {
        return nullptr; // temporary
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

    void vulkan_device_t::destroy_buffer(rhi_buffer_t* buffer)
    {

    }
} // namespace carrot::rhi::vulkan