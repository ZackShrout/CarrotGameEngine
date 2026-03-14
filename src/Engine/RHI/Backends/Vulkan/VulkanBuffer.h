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
        vulkan_buffer_t(const VkDevice device, const size_t size_bytes, const buffer_usage_t usage) noexcept
            : rhi_buffer_t(size_bytes, usage), _device(device) {}

        ~vulkan_buffer_t() override;

        [[nodiscard]] VkBuffer vk_buffer() const noexcept { return _buffer; }

        void set_buffer(const VkBuffer buffer) noexcept { _buffer = buffer; }
        void set_memory(const VkDeviceMemory memory) noexcept { _memory = memory; }

    private:
        VkDevice _device{ VK_NULL_HANDLE };
        VkBuffer _buffer{ VK_NULL_HANDLE };
        VkDeviceMemory _memory{ VK_NULL_HANDLE };
    };
} // namespace carrot::rhi::vulkan
