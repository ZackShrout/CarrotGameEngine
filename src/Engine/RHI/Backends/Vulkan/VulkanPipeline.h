//
// Created by zshrout on 1/9/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

#pragma once

#include "VulkanCommon.h"
#include "VulkanDevice.h"

namespace carrot::assets {
    class shader_file_provider_t;
}

namespace carrot::rhi::vulkan {
    class vulkan_pipeline_t final
    {
    public:
        vulkan_pipeline_t(const vulkan_device_t* device, VkRenderPass render_pass,
                          assets::shader_file_provider_t* shader_files);
        ~vulkan_pipeline_t();

        [[nodiscard]] VkPipeline vk_pipeline() const noexcept { return _pipeline; }
        [[nodiscard]] VkPipelineLayout vk_layout() const noexcept { return _layout; }

    private:
        [[nodiscard]] VkShaderModule create_shader_module(const std::vector<uint32_t>& code) const;

        const vulkan_device_t* _device{ nullptr };
        VkPipelineLayout _layout{ VK_NULL_HANDLE };
        VkPipeline _pipeline{ VK_NULL_HANDLE };
    };
} // namespace carrot::rhi::vulkan
