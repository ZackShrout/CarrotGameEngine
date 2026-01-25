//
// Created by zshrout on 1/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "VulkanCommon.h"
#include "VulkanDevice.h"

namespace carrot::rhi::vulkan {
    class vulkan_render_pass_t final
    {
    public:
        vulkan_render_pass_t(const vulkan_device_t* device, VkFormat color_format);
        ~vulkan_render_pass_t();

        DISABLE_COPY(vulkan_render_pass_t)

        vulkan_render_pass_t(vulkan_render_pass_t&&) noexcept = default;
        vulkan_render_pass_t& operator=(vulkan_render_pass_t&&) noexcept = default;

        [[nodiscard]] VkRenderPass vk_render_pass() const noexcept { return _render_pass; }

    private:
        const vulkan_device_t*  _device{ nullptr };
        VkRenderPass            _render_pass{ VK_NULL_HANDLE };
    };
} // namespace carrot::rhi::vulkan
