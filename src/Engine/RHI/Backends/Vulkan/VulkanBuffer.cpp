//
// Created by zshrout on 3/14/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#include "Core/Pch.h"

#include "VulkanBuffer.h"

namespace carrot::rhi::vulkan {
    vulkan_buffer_t::~vulkan_buffer_t()
    {
        if (_buffer != VK_NULL_HANDLE)
            vkDestroyBuffer(_device, _buffer, nullptr);

        if (_memory != VK_NULL_HANDLE)
            vkFreeMemory(_device, _memory, nullptr);
    }

    bool vulkan_buffer_t::write(const void* data, const size_t size_bytes, const size_t offset_bytes)
    {
        if (data == nullptr)
        {
            LOG_GRAPHICS_ERROR("vulkan_buffer_t::write called with null data");
            return false;
        }

        if (_memory == VK_NULL_HANDLE)
        {
            LOG_GRAPHICS_ERROR("vulkan_buffer_t::write called with invalid buffer memory");
            return false;
        }

        if ((_memory_properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
        {
            LOG_GRAPHICS_ERROR("vulkan_buffer_t::write called on non-host-visible memory");
            return false;
        }

        if (offset_bytes + size_bytes > this->size_bytes())
        {
            LOG_GRAPHICS_ERROR("vulkan_buffer_t::write out of bounds (offset={}, size={}, capacity={})", offset_bytes,
                               size_bytes, this->size_bytes());
            return false;
        }

        void* mapped_data{ nullptr };
        const VkResult map_result{
            vkMapMemory(_device, _memory, static_cast<VkDeviceSize>(offset_bytes),
                        static_cast<VkDeviceSize>(size_bytes), 0, &mapped_data)
        };

        if (map_result != VK_SUCCESS || mapped_data == nullptr)
        {
            LOG_GRAPHICS_ERROR("vkMapMemory failed in vulkan_buffer_t::write (VkResult={})",
                               static_cast<int>(map_result));
            return false;
        }

        std::memcpy(mapped_data, data, size_bytes);

        if ((_memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
        {
            VkMappedMemoryRange range{ };
            range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            range.memory = _memory;
            range.offset = static_cast<VkDeviceSize>(offset_bytes);
            range.size = static_cast<VkDeviceSize>(size_bytes);

            const VkResult flush_result{ vkFlushMappedMemoryRanges(_device, 1, &range) };
            if (flush_result != VK_SUCCESS)
            {
                vkUnmapMemory(_device, _memory);
                LOG_GRAPHICS_ERROR("vkFlushMappedMemoryRanges failed in vulkan_buffer_t::write (VkResult={})",
                                   static_cast<int>(flush_result));
                return false;
            }
        }

        vkUnmapMemory(_device, _memory);
        return true;
    }
} // namespace carrot::rhi::vulkan
