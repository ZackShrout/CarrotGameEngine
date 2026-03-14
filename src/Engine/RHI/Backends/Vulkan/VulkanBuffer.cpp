//
// Created by zshrout on 3/14/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "VulkanBuffer.h"

namespace carrot::rhi::vulkan {
    vulkan_buffer_t::~vulkan_buffer_t()
    {
        if (_buffer != VK_NULL_HANDLE)
            vkDestroyBuffer(_device, _buffer, nullptr);

        if (_memory != VK_NULL_HANDLE)
            vkFreeMemory(_device, _memory, nullptr);
    }
} // namespace carrot::rhi::vulkan
