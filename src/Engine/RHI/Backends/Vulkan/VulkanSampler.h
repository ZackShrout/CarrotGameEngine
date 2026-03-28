//
// Created by Zack Shrout on 3/28/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "RHI/Sampler.h"
#include "VulkanCommon.h"

namespace carrot::rhi::vulkan {
    class vulkan_sampler_t final : public rhi_sampler_t
    {
    public:
        vulkan_sampler_t(VkDevice device, VkSampler sampler, const sampler_desc_t& desc)
            : rhi_sampler_t{ desc }, _device{ device }, _sampler{ sampler } {}

        ~vulkan_sampler_t() override
        {
            if (_sampler != VK_NULL_HANDLE)
            {
                vkDestroySampler(_device, _sampler, nullptr);
                _sampler = VK_NULL_HANDLE;
            }
        }

        [[nodiscard]] VkSampler vk_sampler() const noexcept { return _sampler; }

    private:
        VkDevice _device{ VK_NULL_HANDLE };
        VkSampler _sampler{ VK_NULL_HANDLE };
    };
} // namespace carrot::rhi::vulkan
