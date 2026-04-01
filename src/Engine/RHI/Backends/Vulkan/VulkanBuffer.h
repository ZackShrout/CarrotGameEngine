//
// Created by zshrout on 3/14/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "VulkanCommon.h"
#include "RHI/Buffer.h"

namespace carrot::rhi::vulkan {
    class vulkan_buffer_t final : public rhi_buffer_t
    {
    public:
        vulkan_buffer_t(VkDevice device, const size_t size_bytes, const buffer_usage_t usage) noexcept;
        ~vulkan_buffer_t() override;

        bool write(const void* data, size_t size_bytes, size_t offset_bytes) override;

        [[nodiscard]] VkBuffer vk_buffer() const noexcept { return _buffer; }
        [[nodiscard]] VkDeviceMemory vk_memory() const noexcept { return _memory; }

        void set_buffer(VkBuffer buffer) noexcept { _buffer = buffer; }
        void set_memory(VkDeviceMemory memory) noexcept { _memory = memory; }
        void set_memory_properties(const VkMemoryPropertyFlags properties) noexcept { _memory_properties = properties; }

    private:
        VkDevice _device{ VK_NULL_HANDLE };
        VkBuffer _buffer{ VK_NULL_HANDLE };
        VkDeviceMemory _memory{ VK_NULL_HANDLE };
        VkMemoryPropertyFlags _memory_properties{ 0 };
    };
} // namespace carrot::rhi::vulkan
